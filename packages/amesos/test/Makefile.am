#@HEADER
# ************************************************************************
# 
#               Epetra: Linear Algebra Services Package 
#                 Copyright (2001) Sandia Corporation
# 
# Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
# license for use of this work by or on behalf of the U.S. Government.
# 
# This library is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2.1 of the
# License, or (at your option) any later version.
#  
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#  
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301
# USA
# Questions? Contact Michael A. Heroux (maherou@sandia.gov) 
# 
# ************************************************************************
#@HEADER

# The following line helps the test harness recover from build errors.

all-local:

if HAVE_PERFORMANCE
PERF_SUBDIR = Test_Performance
else
PERF_SUBDIR =
endif

SUBDIRS = scripts \
	  Test_Basic \
	  Test_Epetra_RowMatrix Test_Epetra_CrsMatrix \
	  Test_MultipleSolves \
	  Test_Detailed \
	  Test_LAPACK \
	  Test_KLU \
	  Test_UMFPACK \
	  Test_Singular \
	  Test_SuperLU \
	  Test_SuperLU_DIST \
	  Test_MUMPS \
	  Test_DSCPACK TestOptions ${PERF_SUBDIR}

EXTRA_DIST = definition

