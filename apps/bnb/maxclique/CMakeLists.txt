foreach(nwords 4 8 16)
add_hpx_executable(maxclique-${nwords}
  SOURCES main.cpp DimacsParser.cpp
  COMPILE_FLAGS "-DNWORDS=${nwords}"
  DEPENDENCIES YewPar_lib)
endforeach(nwords)
