# Copyright 2020 by Cliff Green
#
# https://github.com/connectivecpp/chops-net-ip
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required ( VERSION 3.9 )

project ( chops-net-ip-doc VERSION 1.0 LANGUAGES CXX )

find_package ( Doxygen 
               OPTIONAL_COMPONENTS dot mscgen dia
             )

if (DOXYGEN_FOUND)
  set ( DOXYGEN_PROJECT_NAME "Chops Net IP API Reference" )
  set ( DOXYGEN_PROJECT_BRIEF "Documentation of the classes and functions residing in Chops Net IP" )
  set ( DOXYGEN_EXCLUDE_PATTERNS */detail/* )
  doxygen_add_docs ( docs
                     include
                     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                     COMMENT "Document generation command"
                   )

else (DOXYGEN_FOUND)
  message("Doxygen needs to be installed to generate the doxygen documentation")
endif (DOXYGEN_FOUND)



# end of file

