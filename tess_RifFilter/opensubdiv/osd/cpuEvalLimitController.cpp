//
//   Copyright 2013 Pixar
//
//   Licensed under the Apache License, Version 2.0 (the "Apache License")
//   with the following modification; you may not use this file except in
//   compliance with the Apache License and the following modification to it:
//   Section 6. Trademarks. is deleted and replaced with:
//
//   6. Trademarks. This License does not grant permission to use the trade
//      names, trademarks, service marks, or product names of the Licensor
//      and its affiliates, except as required to comply with Section 4(c) of
//      the License and to reproduce the content of the NOTICE file.
//
//   You may obtain a copy of the Apache License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the Apache License with the above modification is
//   distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
//   KIND, either express or implied. See the Apache License for the specific
//   language governing permissions and limitations under the Apache License.
//

#include "../osd/cpuEvalLimitController.h"
#include "../osd/cpuEvalLimitKernel.h"
#include "../far/patchTables.h"

namespace OpenSubdiv {
namespace OPENSUBDIV_VERSION {

OsdCpuEvalLimitController::OsdCpuEvalLimitController() {
}

OsdCpuEvalLimitController::~OsdCpuEvalLimitController() {
}


// normalize & rotate (u,v) to the sub-patch
inline void
computeSubPatchCoords( OsdCpuEvalLimitContext * context, unsigned int patchIdx, real & u, real & v ) {

    FarPatchParam::BitField bits = context->GetPatchBitFields()[ patchIdx ];

    bits.Normalize( u, v );

    bits.Rotate( u, v );
}

// Vertex interpolation of a sample at the limit
int
OsdCpuEvalLimitController::EvalLimitSample( OpenSubdiv::OsdEvalCoords const & coord,
                                            OsdCpuEvalLimitContext * context,
                                            OsdVertexBufferDescriptor const & outDesc,
                                            real * outQ,
                                            real * outDQU,
                                            real * outDQV,
                                            real * outDQUDQU,
                                            real * outDQVDQV,
                                            real * outDQUDQV) const {

    real u=coord.u,
          v=coord.v;

    FarPatchMap::Handle const * handle = context->GetPatchMap().FindPatch( coord.face, u, v );

    // the map may not be able to return a handle if there is a hole or the face
    // index is incorrect
    if (not handle)
        return 0;

    computeSubPatchCoords(context, handle->patchIdx, u, v);

    FarPatchTables::PatchArray const & parray = context->GetPatchArrayVector()[ handle->patchArrayIdx ];

    unsigned int const * cvs = &context->GetControlVertices()[ parray.GetVertIndex() + handle->vertexOffset ];

    VertexData const & vertexData = _currentBindState.vertexData;

    if (vertexData.in) {
    
        real * out   = outQ ? outQ + outDesc.offset : 0,
                * outDu = outDQU ? outDQU + outDesc.offset : 0,
                * outDv = outDQV ? outDQV + outDesc.offset : 0,
                * outDuDu = outDQUDQU ? outDQUDQU + outDesc.offset : 0,
                * outDvDv = outDQVDQV ? outDQVDQV + outDesc.offset : 0,
                * outDuDv = outDQUDQV ? outDQUDQV + outDesc.offset : 0;

        switch( parray.GetDescriptor().GetType() ) {

            case FarPatchTables::REGULAR  : evalBSpline( v, u, cvs,
                                                         vertexData.inDesc,
                                                         vertexData.in,
                                                         outDesc,
                                                         out, outDu, outDv, outDuDu, outDvDv, outDuDv );
                                            break;

            case FarPatchTables::BOUNDARY : evalBoundary( v, u, cvs,
                                                          vertexData.inDesc,
                                                          vertexData.in,
                                                          outDesc,
                                                          out, outDu, outDv );
                                            break;

            case FarPatchTables::CORNER   : evalCorner( v, u, cvs,
                                                        vertexData.inDesc,
                                                        vertexData.in,
                                                        outDesc,
                                                        out, outDu, outDv );
                                            break;


            case FarPatchTables::GREGORY  : evalGregory( v, u, cvs,
                                                         &context->GetVertexValenceTable()[0],
                                                         &context->GetQuadOffsetTable()[ parray.GetQuadOffsetIndex() + handle->vertexOffset ],
                                                         context->GetMaxValence(),
                                                         vertexData.inDesc,
                                                         vertexData.in,
                                                         outDesc,
                                                         out, outDu, outDv );
                                            break;

            case FarPatchTables::GREGORY_BOUNDARY :
                                            evalGregoryBoundary( v, u, cvs,
                                                                 &context->GetVertexValenceTable()[0],
                                                                 &context->GetQuadOffsetTable()[ parray.GetQuadOffsetIndex() + handle->vertexOffset ],
                                                                 context->GetMaxValence(),
                                                                 vertexData.inDesc,
                                                                 vertexData.in,
                                                                 outDesc,
                                                                 out, outDu, outDv );
                                            break;

            default:
                assert(0);
        }
    }

    return 1;
}

// Vertex interpolation of samples at the limit
int
OsdCpuEvalLimitController::_EvalLimitSample( OpenSubdiv::OsdEvalCoords const & coords,
                                             OsdCpuEvalLimitContext * context,
                                             unsigned int index ) const {
    real u=coords.u,
          v=coords.v;

    FarPatchMap::Handle const * handle = context->GetPatchMap().FindPatch( coords.face, u, v );

    // the map may not be able to return a handle if there is a hole or the face
    // index is incorrect
    if (not handle)
        return 0;

    computeSubPatchCoords(context, handle->patchIdx, u, v);

    FarPatchTables::PatchArray const & parray = context->GetPatchArrayVector()[ handle->patchArrayIdx ];

    unsigned int const * cvs = &context->GetControlVertices()[ parray.GetVertIndex() + handle->vertexOffset ];

    VertexData const & vertexData = _currentBindState.vertexData;

    if (vertexData.in) {

        int offset = vertexData.outDesc.stride * index;

        if (vertexData.out) {

            real * out   = vertexData.out+offset,
                  * outDu = vertexData.outDu ? vertexData.outDu+offset : 0,
                  * outDv = vertexData.outDv ? vertexData.outDv+offset : 0,
                    * outDuDu = vertexData.outDuDu ? vertexData.outDuDu+offset : 0,
                    * outDvDv = vertexData.outDvDv ? vertexData.outDvDv+offset : 0,
                    * outDuDv = vertexData.outDuDv ? vertexData.outDuDv+offset : 0;

            // Based on patch type - go execute interpolation
            switch( parray.GetDescriptor().GetType() ) {

                case FarPatchTables::REGULAR  : evalBSpline( v, u, cvs,
                                                             vertexData.inDesc,
                                                             vertexData.in,
                                                             vertexData.outDesc,
                                                             out, outDu, outDv, outDuDu, outDvDv, outDuDv );
                                                break;

                case FarPatchTables::BOUNDARY : evalBoundary( v, u, cvs,
                                                              vertexData.inDesc,
                                                              vertexData.in,
                                                              vertexData.outDesc,
                                                              out, outDu, outDv );
                                                break;

                case FarPatchTables::CORNER   : evalCorner( v, u, cvs,
                                                            vertexData.inDesc,
                                                            vertexData.in,
                                                            vertexData.outDesc,
                                                            out, outDu, outDv );
                                                break;


                case FarPatchTables::GREGORY  : evalGregory( v, u, cvs,
                                                             &context->GetVertexValenceTable()[0],
                                                             &context->GetQuadOffsetTable()[ parray.GetQuadOffsetIndex() + handle->vertexOffset ],
                                                             context->GetMaxValence(),
                                                             vertexData.inDesc,
                                                             vertexData.in,
                                                             vertexData.outDesc,
                                                             out, outDu, outDv );
                                                break;

                case FarPatchTables::GREGORY_BOUNDARY :
                                                evalGregoryBoundary( v, u, cvs,
                                                                     &context->GetVertexValenceTable()[0],
                                                                     &context->GetQuadOffsetTable()[ parray.GetQuadOffsetIndex() + handle->vertexOffset ],
                                                                     context->GetMaxValence(),
                                                                     vertexData.inDesc,
                                                                     vertexData.in,
                                                                     vertexData.outDesc,
                                                                     out, outDu, outDv );
                                                break;

                default:
                    assert(0);
            }
        }
    }

    VaryingData const & varyingData = _currentBindState.varyingData;

    if (varyingData.in and varyingData.out) {

        static int indices[5][4] = { {5, 6,10, 9},  // regular
                                     {1, 2, 6, 5},  // boundary
                                     {1, 2, 5, 4},  // corner
                                     {0, 1, 2, 3},  // gregory
                                     {0, 1, 2, 3} };// gregory boundary

        int type = (int)(parray.GetDescriptor().GetType() - FarPatchTables::REGULAR);

        int offset = varyingData.outDesc.stride * index;

        unsigned int zeroRing[4] = { cvs[indices[type][0]],
                                     cvs[indices[type][1]],
                                     cvs[indices[type][2]],
                                     cvs[indices[type][3]]  };

        evalBilinear( v, u, zeroRing,
                      varyingData.inDesc,
                      varyingData.in,
                      varyingData.outDesc,
                      varyingData.out+offset);

    }

    // Note : currently we only support bilinear boundary interpolation rules
    // for face-varying data. Although Hbr supports 3 additional smooth rule
    // sets, the feature-adaptive patch interpolation code currently does not
    // support them, and neither does this EvalContext.

    FacevaryingData const & facevaryingData = _currentBindState.facevaryingData;

    if (facevaryingData.out) {

        std::vector<real> const & fvarData = context->GetFVarData();

        if (not fvarData.empty()) {

            int offset = facevaryingData.outDesc.stride * index;

            static unsigned int zeroRing[4] = {0,1,2,3};

            evalBilinear( v, u, zeroRing,
                          facevaryingData.inDesc,
                          &fvarData[ handle->patchIdx * 4 * context->GetFVarWidth() ],
                          facevaryingData.outDesc,
                          facevaryingData.out+offset);
        }
    }

    return 1;
}

}  // end namespace OPENSUBDIV_VERSION
}  // end namespace OpenSubdiv
