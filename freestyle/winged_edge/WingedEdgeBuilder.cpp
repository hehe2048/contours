
//
//  Copyright (C) : Please refer to the COPYRIGHT file distributed 
//   with this source distribution. 
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
///////////////////////////////////////////////////////////////////////////////

#include "../geometry/GeomUtils.h"
#include "../scene_graph/NodeShape.h"
#include "WingedEdgeBuilder.h"
#include <set>

#include <assert.h>
using namespace std;

void WingedEdgeBuilder::visitIndexedFaceSet(IndexedFaceSet& ifs) {

    //      printf("visitIndexedFaceSet check:\n");
    //      printf("%d %d %d %d\n",ifs.faceUserData()[0],ifs.faceUserData()[1],ifs.faceUserData()[2],ifs.faceUserData()[3]);

    WShape *shape = new WShape;
    buildWShape(*shape, ifs);
    shape->SetId(ifs.getId().getFirst());
    shape->SetMeshSilhouettes(ifs.meshSilhouettes());
    //ifs.SetId(shape->GetId());
}

void WingedEdgeBuilder::visitNodeShape(NodeShape& ns) {
    //Sets the current material to iShapeode->material:
    _current_material = &(ns.material());
}

void WingedEdgeBuilder::visitNodeTransform(NodeTransform& tn) {
    if(!_current_matrix) {
        _current_matrix = new Matrix44r(tn.matrix());
        return;
    }

    _matrices_stack.push_back(_current_matrix);
    Matrix44r *new_matrix = new Matrix44r(*_current_matrix * tn.matrix());
    _current_matrix = new_matrix;
}

void WingedEdgeBuilder::visitNodeTransformAfter(NodeTransform&) {
    if(_current_matrix)
        delete _current_matrix;

    if(_matrices_stack.empty()) {
        _current_matrix = NULL;
        return;
    }

    _current_matrix = _matrices_stack.back();
    _matrices_stack.pop_back();
}

void WingedEdgeBuilder::buildWShape(WShape& shape, IndexedFaceSet& ifs) {
    unsigned	vsize = ifs.vsize();
    unsigned	nsize = ifs.nsize();
    unsigned	tsize = ifs.tsize();

    const real*	vertices = ifs.vertices();
    const real*	normals = ifs.normals();
    const real*	texCoords = ifs.texCoords();

    real*		new_vertices;
    real*		new_normals;

    new_vertices = new real[vsize];
    new_normals = new real[nsize];

    const int * faceUserData = ifs.faceUserData();
    const float * vertexUserData = ifs.vertexUserData();

    //		printf("buildWShape:\n");
    //		for(int i=0;i<10;i++)
    //		  printf("%d ",ifs.faceUserData()[i]);
    //		printf("\n");
    // transform coordinates from local to world system
    if(_current_matrix) {
        transformVertices(vertices, vsize, *_current_matrix, new_vertices);
        transformNormals(normals, nsize, *_current_matrix, new_normals);
    }
    else {
        memcpy(new_vertices, vertices, vsize * sizeof(*new_vertices));
        memcpy(new_normals, normals, nsize * sizeof(*new_normals));

        assert(new_vertices[0] == vertices[0]);
        assert(new_vertices[1] == vertices[1]);
    }

    const IndexedFaceSet::TRIANGLES_STYLE*	faceStyle = ifs.trianglesStyle();

    vector<Material> materials;
    if(ifs.msize()){
        const Material*const* mats = ifs.materials();
        for(unsigned i=0; i<ifs.msize(); ++i)
            materials.push_back(*(mats[i]));
        shape.SetMaterials(materials);
    }

    //  const Material * mat = (ifs.material());
    //  if (mat)
    //    shape.SetMaterial(*mat);
    //  else if(_current_material)
    //    shape.SetMaterial(*_current_material);

    // sets the current WShape to shape
    _current_wshape = &shape;

    // create a WVertex for each vertex
    buildWVertices(shape, new_vertices, new_normals, vsize, vertexUserData);

    const unsigned*	vindices = ifs.vindices();
    const unsigned*	nindices = ifs.nindices();
    const unsigned*	tindices = 0;
    if(ifs.tsize()){
        tindices = ifs.tindices();
    }

    const unsigned *mindices = 0;
    if(ifs.msize())
        mindices = ifs.mindices();
    const unsigned*	numVertexPerFace = ifs.numVertexPerFaces();
    const unsigned	numfaces = ifs.numFaces();

    for (unsigned index = 0; index < numfaces; index++) {
        //    if (index < 10)
        //      printf("buildWShape: faceUserData[%d]  = %d\n", index, faceUserData[index]);

        switch(faceStyle[index]) {
        case IndexedFaceSet::TRIANGLE_STRIP:
            buildTriangleStrip(new_vertices,
                               new_normals,
                               materials,
                               texCoords,
                               vindices,
                               nindices,
                               mindices,
                               tindices,
                               numVertexPerFace[index]);
            break;
        case IndexedFaceSet::TRIANGLE_FAN:
            buildTriangleFan(new_vertices,
                             new_normals,
                             materials,
                             texCoords,
                             vindices,
                             nindices,
                             mindices,
                             tindices,
                             numVertexPerFace[index]);
            break;
        case IndexedFaceSet::TRIANGLES:
            buildTriangles(new_vertices,
                           new_normals,
                           materials,
                           texCoords,
                           vindices,
                           nindices,
                           mindices,
                           tindices,
                           numVertexPerFace[index], faceUserData[index]);
            break;
        }
        vindices += numVertexPerFace[index];
        nindices += numVertexPerFace[index];
        if(mindices)
            mindices += numVertexPerFace[index];
        if(tindices)
            tindices += numVertexPerFace[index];
    }

    delete[] new_vertices;
    delete[] new_normals;

    // compute bbox
    shape.ComputeBBox();
    // compute mean edge size:
    shape.ComputeMeanEdgeSize();

    // Parse the built winged-edge shape to update post-flags
    set<Vec3r> normalsSet;
    vector<WVertex*>& wvertices = shape.GetVertexList();
    for(vector<WVertex*>::iterator wv=wvertices.begin(), wvend=wvertices.end();
        wv!=wvend;
        ++wv){
        if((*wv)->isBoundary())
            continue;
        normalsSet.clear();
        WVertex::face_iterator fit = (*wv)->faces_begin();
        WVertex::face_iterator fitend = (*wv)->faces_end();
        while(fit!=fitend){
            WFace *face = *fit;
            normalsSet.insert(face->GetVertexNormal(*wv));
            if(normalsSet.size()!=1){
                break;
            }
            ++fit;
        }
        if(normalsSet.size()!=1){
            //      Vec3r n1 = *(normalsSet.begin());
            //      Vec3r n2 = *(++normalsSet.begin());

            //      printf("non-smooth.  n0 = [%f %f %f], n1 = [%f %f %f]\n",
            //	     n1[0], n1[1], n1[2], n2[0], n2[1], n2[2]);

            (*wv)->SetSmooth(false);
        }
    }
    // Adds the new WShape to the WingedEdge structure
    _winged_edge->addWShape(&shape);
}

void WingedEdgeBuilder::buildWVertices(WShape& shape,
                                       const real *vertices, const real *normals,
                                       unsigned vsize, const float * vertexUserData) {
    WVertex *vertex;
    for (unsigned i = 0; i < vsize; i += 3) {
        vertex = new WVertex(Vec3r(vertices[i],
                                   vertices[i + 1],
                                   vertices[i + 2]));
        vertex->SetId(i / 3);
        vertex->SetNormal(Vec3r(normals[i],
                                normals[i + 1],
                                normals[i + 2]));
        vertex->SetSurfaceNdotV(vertexUserData[i/3]);
        shape.AddVertex(vertex);
    }
}

void WingedEdgeBuilder::buildTriangleStrip( const real *vertices, 
                                            const real *normals,
                                            vector<Material>& iMaterials,
                                            const real *texCoords,
                                            const unsigned *vindices,
                                            const unsigned *nindices,
                                            const unsigned *mindices,
                                            const unsigned *tindices,
                                            const unsigned nvertices) {
    unsigned nDoneVertices = 2; // number of vertices already treated
    unsigned nTriangle = 0;     // number of the triangle currently being treated
    //int nVertex = 0;       // vertex number

    WShape* currentShape = _current_wshape; // the current shape being built
    vector<WVertex *> triangleVertices;
    vector<Vec3r> triangleNormals;
    vector<Vec2r> triangleTexCoords;

    while(nDoneVertices < nvertices)
    {
        //clear the vertices list:
        triangleVertices.clear();
        //Then rebuild it:
        if(0 == nTriangle%2) // if nTriangle is even
        {
            triangleVertices.push_back(currentShape->GetVertexList()[vindices[nTriangle]/3]);
            triangleVertices.push_back(currentShape->GetVertexList()[vindices[nTriangle+1]/3]);
            triangleVertices.push_back(currentShape->GetVertexList()[vindices[nTriangle+2]/3]);

            triangleNormals.push_back(Vec3r(normals[nindices[nTriangle]],normals[nindices[nTriangle]+1], normals[nindices[nTriangle]+2]));
            triangleNormals.push_back(Vec3r(normals[nindices[nTriangle+1]],normals[nindices[nTriangle+1]+1],normals[nindices[nTriangle+1]+2]));
            triangleNormals.push_back(Vec3r(normals[nindices[nTriangle+2]], normals[nindices[nTriangle+2]+1], normals[nindices[nTriangle+2]+2]));

            if(texCoords){
                triangleTexCoords.push_back(Vec2r(texCoords[tindices[nTriangle]],texCoords[tindices[nTriangle]+1]));
                triangleTexCoords.push_back(Vec2r(texCoords[tindices[nTriangle+1]],texCoords[tindices[nTriangle+1]+1]));
                triangleTexCoords.push_back(Vec2r(texCoords[tindices[nTriangle+2]], texCoords[tindices[nTriangle+2]+1]));
            }
        }
        else                 // if nTriangle is odd
        {
            triangleVertices.push_back(currentShape->GetVertexList()[vindices[nTriangle]/3]);
            triangleVertices.push_back(currentShape->GetVertexList()[vindices[nTriangle+2]/3]);
            triangleVertices.push_back(currentShape->GetVertexList()[vindices[nTriangle+1]/3]);

            triangleNormals.push_back(Vec3r(normals[nindices[nTriangle]],normals[nindices[nTriangle]+1], normals[nindices[nTriangle]+2]));
            triangleNormals.push_back(Vec3r(normals[nindices[nTriangle+2]],normals[nindices[nTriangle+2]+1],normals[nindices[nTriangle+2]+2]));
            triangleNormals.push_back(Vec3r(normals[nindices[nTriangle+1]], normals[nindices[nTriangle+1]+1], normals[nindices[nTriangle+1]+2]));

            if(texCoords){
                triangleTexCoords.push_back(Vec2r(texCoords[tindices[nTriangle]],texCoords[tindices[nTriangle]+1]));
                triangleTexCoords.push_back(Vec2r(texCoords[tindices[nTriangle+2]],texCoords[tindices[nTriangle+2]+1]));
                triangleTexCoords.push_back(Vec2r(texCoords[tindices[nTriangle+1]], texCoords[tindices[nTriangle+1]+1]));
            }
        }
        if(mindices)
            currentShape->MakeFace(triangleVertices, triangleNormals, triangleTexCoords, mindices[nTriangle/3]);
        else
            currentShape->MakeFace(triangleVertices, triangleNormals, triangleTexCoords, 0);
        nDoneVertices++; // with a strip, each triangle is one vertex more
        nTriangle++;
    }
}

void WingedEdgeBuilder::buildTriangleFan( const real *vertices, 
                                          const real *normals,
                                          vector<Material>&  iMaterials,
                                          const real *texCoords,
                                          const unsigned *vindices,
                                          const unsigned *nindices,
                                          const unsigned *mindices,
                                          const unsigned *tindices,
                                          const unsigned nvertices) {
    // Nothing to be done
}

void WingedEdgeBuilder::buildTriangles(const real *vertices, 
                                       const real *normals,
                                       vector<Material>&  iMaterials,
                                       const real *texCoords,
                                       const unsigned *vindices,
                                       const unsigned *nindices,
                                       const unsigned *mindices,
                                       const unsigned *tindices,
                                       const unsigned nvertices,
                                       const int  faceUserData) {
    WShape * currentShape = _current_wshape; // the current shape begin built
    vector<WVertex *> triangleVertices;
    vector<Vec3r> triangleNormals;
    vector<Vec2r> triangleTexCoords;

    // Each triplet of vertices is considered as an independent triangle
    for(unsigned i = 0; i < nvertices / 3; i++)
    {
        triangleVertices.push_back(currentShape->GetVertexList()[vindices[3*i]/3]);
        triangleVertices.push_back(currentShape->GetVertexList()[vindices[3*i+1]/3]);
        triangleVertices.push_back(currentShape->GetVertexList()[vindices[3*i+2]/3]);

        triangleNormals.push_back(Vec3r(normals[nindices[3*i]],normals[nindices[3*i]+1], normals[nindices[3*i]+2]));
        triangleNormals.push_back(Vec3r(normals[nindices[3*i+1]],normals[nindices[3*i+1]+1],normals[nindices[3*i+1]+2]));
        triangleNormals.push_back(Vec3r(normals[nindices[3*i+2]], normals[nindices[3*i+2]+1], normals[nindices[3*i+2]+2]));

        if(texCoords){
            triangleTexCoords.push_back(Vec2r(texCoords[tindices[3*i]],texCoords[tindices[3*i]+1]));
            triangleTexCoords.push_back(Vec2r(texCoords[tindices[3*i+1]],texCoords[tindices[3*i+1]+1]));
            triangleTexCoords.push_back(Vec2r(texCoords[tindices[3*i+2]], texCoords[tindices[3*i+2]+1]));
        }
    }

    int mindex = (mindices ? mindices[0] : 0) ;

    currentShape->MakeFace(triangleVertices, triangleNormals, triangleTexCoords, mindex, faceUserData);

    //	if(mindices)
    //	  currentShape->MakeFace(triangleVertices, triangleNormals, triangleTexCoords, mindices[0]);
    //	else
    //	  currentShape->MakeFace(triangleVertices, triangleNormals, triangleTexCoords, 0);

}

void WingedEdgeBuilder::transformVertices(const real *vertices,
                                          unsigned vsize,
                                          const Matrix44r& transform,
                                          real *res) {
    const real *v = vertices;
    real *pv = res;
    
    for (unsigned i = 0; i < vsize / 3; i++) {
        HVec3r hv_tmp(v[0], v[1], v[2]);
        HVec3r hv(transform * hv_tmp);
        for (unsigned j = 0; j < 3; j++)
            pv[j] = hv[j] / hv[3];
        v += 3;
        pv += 3;
    }
}

void WingedEdgeBuilder::transformNormals(const real *normals,
                                         unsigned nsize,
                                         const Matrix44r& transform,
                                         real* res) {
    const real *n = normals;
    real *pn = res;
    
    for (unsigned i = 0; i < nsize / 3; i++) {
        Vec3r hn(n[0], n[1], n[2]);
        hn = GeomUtils::rotateVector(transform, hn);
        for (unsigned j = 0; j < 3; j++)
            pn[j] = hn[j];
        n += 3;
        pn += 3;
    }
}
