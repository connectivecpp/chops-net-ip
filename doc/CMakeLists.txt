# Copyright 2020 by Cliff Green
#
# https://github.com/connectivecpp/chops-net-ip
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required ( VERSION 3.9 )

project ( chops-net-ip-doc VERSION 1.0 LANGUAGES CXX )

set ( doxygen_proj_name "Chops Net IP Reference" )
include ( "${cmake_all_repos_include_dir}/doxygen_gen.cmake" )

# end of file

