foreach(nwords 4 8 16)
add_hpx_executable(maxclique-${nwords}
  SOURCES main.cpp DimacsParser.cpp
  COMPONENT_DEPENDENCIES incumbent workqueue
  COMPILE_FLAGS "-DNWORDS=${nwords}"
  DEPENDENCIES workstealing_lib)
endforeach(nwords)
