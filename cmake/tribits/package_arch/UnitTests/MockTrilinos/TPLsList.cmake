# @HEADER
# ************************************************************************
#
#            TriBITS: Tribial Build, Integrate, and Test System
#                    Copyright 2013 Sandia Corporation
#
# Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
# the U.S. Government retains certain rights in this software.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
#
# 3. Neither the name of the Corporation nor the names of the
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# ************************************************************************
# @HEADER


# Just used for unit testing
TRIBITS_DEFINE_REPOSITORY_TPLS_FINDMODS_CLASSIFICATIONS(
  MPI            cmake/TPLs/    PT
  BLAS           cmake/TPLs/    PT
  LAPACK         cmake/TPLs/    PT
  Boost          cmake/TPLs/    ST
  Scotch         cmake/TPLs/    ST
  METIS          cmake/TPLs/    TS
  ParMETIS       cmake/TPLs/    ST
  CppUnit        cmake/TPLs/    ST
  ADOLC          cmake/TPLs/    ST
  ADIC           cmake/TPLs/    EX
  TVMET          cmake/TPLs/    ST
  y12m           cmake/TPLs/    ST
  SuperLUDist    cmake/TPLs/    ST
  SuperLU        cmake/TPLs/    ST
  UMFPACK        cmake/TPLs/    ST
  AMD            cmake/TPLs/    TS
  PETSC          cmake/TPLs/    ST
  MUMPS          cmake/TPLs/    ST
  DUMMY          cmake/TPLs/    ST
  )
