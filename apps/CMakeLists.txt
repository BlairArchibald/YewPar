if(YEWPAR_BUILD_BNB_APPS)
  add_subdirectory(bnb)
endif(YEWPAR_BUILD_BNB_APPS)

if(YEWPAR_BUILD_ENUMERATION_APPS)
  add_subdirectory(enumeration)
endif(YEWPAR_BUILD_ENUMERATION_APPS)

if(YEWPAR_BUILD_DNC_APPS)
  add_subdirectory(dnc)
endif(YEWPAR_BUILD_DNC_APPS)

add_subdirectory(decision)
