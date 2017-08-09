foreach(nwords 4 8 16)
add_hpx_executable(mcs-${nwords}
  SOURCES main.cpp VFParser.cpp
  COMPILE_FLAGS "-DNWORDS=${nwords}"
  DEPENDENCIES YewPar_lib)
endforeach(nwords)
