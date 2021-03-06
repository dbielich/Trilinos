// Copyright(C) 1999-2010 National Technology & Engineering Solutions
// of Sandia, LLC (NTESS).  Under the terms of Contract DE-NA0003525 with
// NTESS, the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//
//     * Neither the name of NTESS nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "Ioss_DBUsage.h"          // for DatabaseUsage
#include "Ioss_IOFactory.h"        // for IOFactory
#include <adios/Ioad_DatabaseIO.h> // for DatabaseIO
#include <adios/Ioad_IOFactory.h>
#include <stddef.h> // for nullptr
#include <string>   // for string

#include <adios2/common/ADIOSConfig.h>
#include <fmt/ostream.h>

namespace Ioss {
  class PropertyManager;
}

namespace Ioad {

  const IOFactory *IOFactory::factory()
  {
    static IOFactory registerThis;
    return &registerThis;
  }

  IOFactory::IOFactory() : Ioss::IOFactory("adios") { Ioss::IOFactory::alias("adios", "adios2"); }

  Ioss::DatabaseIO *IOFactory::make_IO(const std::string &filename, Ioss::DatabaseUsage db_usage,
                                       MPI_Comm                     communicator,
                                       const Ioss::PropertyManager &properties) const
  {
    return new DatabaseIO(nullptr, filename, db_usage, communicator, properties);
  }

  void IOFactory::show_config() const
  {
    fmt::print(stderr, "\tADIOS2 Library Version: {}.{}.{}\n", ADIOS2_VERSION_MAJOR,
               ADIOS2_VERSION_MINOR, ADIOS2_VERSION_PATCH);
#if defined(ADIOS2_HAVE_BZIP2)
    fmt::print(stderr, "\t\tBZip2 (http://www.bzip.org/) compression enabled\n");
#endif

#if defined(ADIOS2_HAVE_ZFP)
    fmt::print(stderr, "\t\tZFP (https://github.com/LLNL/zfp) compression enabled\n");
#endif

#if defined(ADIOS2_HAVE_SZ)
    fmt::print(stderr, "\t\tSZ compression enabled\n");
#endif

#if defined(ADIOS2_HAVE_MPI)
    fmt::print(stderr, "\t\tParallel (MPI) enabled\n");
#else
    fmt::print(stderr, "\t\tParallel *NOT* enabled\n");
#endif

#if defined(ADIOS2_HAVE_SST)
    fmt::print(stderr, "\t\tStaging engine enabled\n");
#else
    fmt::print(stderr, "\t\tStaging engine *NOT* enabled\n");
#endif

#if defined(ADIOS2_HAVE_HDF5)
    fmt::print(stderr, "\t\tHDF5 (https://www.hdfgroup.org) engine enabled\n\n");
#else
    fmt::print(stderr, "\t\tHDF5 engine *NOT* enabled\n\n");
#endif

    /* #if defined(ADIOS2_HAVE_ZEROMQ) */
    /* #if defined(ADIOS2_HAVE_WDM) */
    /* #if defined(ADIOS2_HAVE_DATAMAN) */
    /* #if defined(ADIOS2_HAVE_MGARD) */
    /* #undef ADIOS2_HAVE_PYTHON */
    /* #define ADIOS2_HAVE_FORTRAN */
    /* #define ADIOS2_HAVE_SYSVSHMEM */
    /* #undef ADIOS2_HAVE_ENDIAN_REVERSE */
  }
} // namespace Ioad
