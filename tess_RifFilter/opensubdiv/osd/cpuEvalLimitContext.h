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

#ifndef OSD_CPU_EVAL_LIMIT_CONTEXT_H
#define OSD_CPU_EVAL_LIMIT_CONTEXT_H

#include "../version.h"

#include "../osd/evalLimitContext.h"
#include "../osd/vertexDescriptor.h"
#include "../far/patchTables.h"
#include "../far/patchMap.h"

#include <map>
#include <stdio.h>

namespace OpenSubdiv {
namespace OPENSUBDIV_VERSION {

class OsdCpuEvalLimitContext : public OsdEvalLimitContext {
public:

    /// \brief Factory
    /// Returns an EvalLimitContext from the given far patch tables.
    /// Note : the patchtables is expected to be feature-adaptive and have ptex
    ///        coordinates tables.
    /// 
    /// @param patchTables      a pointer to an initialized FarPatchTables
    ///
    /// @param requireFVarData  flag for generating face-varying data
    ///
    static OsdCpuEvalLimitContext * Create(FarPatchTables const *patchTables,
                                           bool requireFVarData=false);

    virtual ~OsdCpuEvalLimitContext();


    /// Returns the vector of patch arrays
    const FarPatchTables::PatchArrayVector & GetPatchArrayVector() const {
        return _patchArrays;
    }
    
    /// Returns the vector of per-patch parametric data
    const std::vector<FarPatchParam::BitField> & GetPatchBitFields() const {
        return _patchBitFields;
    }

    /// The ordered array of control vertex indices for all the patches
    const std::vector<unsigned int> & GetControlVertices() const {
        return _patches;
    }

    /// Returns the vertex-valence buffer used for Gregory patch computations
    FarPatchTables::VertexValenceTable const & GetVertexValenceTable() const {
        return _vertexValenceTable;
    }

    /// Returns the Quad-Offsets buffer used for Gregory patch computations
    FarPatchTables::QuadOffsetTable const & GetQuadOffsetTable() const {
        return _quadOffsetTable;
    }
    
    /// Returns the face-varying data patch table
    std::vector<real> const & GetFVarData() const {
        return _fvarData;
    }
    
    /// Returns the number of floats in a datum of the face-varying data table
    int GetFVarWidth() const {
        return _fvarwidth;
    }

    /// Returns a map that can connect a faceId to a list of children patches
    FarPatchMap const & GetPatchMap() const {
        return *_patchMap;
    }

    /// Returns the highest valence of the vertices in the buffers
    int GetMaxValence() const {
        return _maxValence;
    }

protected:
    explicit OsdCpuEvalLimitContext(FarPatchTables const *patchTables, bool requireFVarData);

private:

    // Topology data for a mesh
    FarPatchTables::PatchArrayVector     _patchArrays;    // patch descriptor for each patch in the mesh
    FarPatchTables::PTable               _patches;        // patch control vertices
    std::vector<FarPatchParam::BitField> _patchBitFields; // per-patch parametric info
    
    FarPatchTables::VertexValenceTable   _vertexValenceTable; // extra Gregory patch data buffers
    FarPatchTables::QuadOffsetTable      _quadOffsetTable;

    std::vector<real>                   _fvarData;

    FarPatchMap * _patchMap;           // map of the sub-patches given a face index

    int _maxValence, 
        _fvarwidth;
};


} // end namespace OPENSUBDIV_VERSION
using namespace OPENSUBDIV_VERSION;

} // end namespace OpenSubdiv

#endif /* OSD_CPU_EVAL_LIMIT_CONTEXT_H */
