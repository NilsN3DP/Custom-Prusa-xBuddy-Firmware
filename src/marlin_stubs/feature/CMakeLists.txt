if(HAS_CHAMBER_API)
  add_subdirectory(chamber)
endif()

if(HAS_CHAMBER_VENTS)
  add_subdirectory(automatic_chamber_vents)
endif()

if(HAS_AUTOFEEDER)
  add_subdirectory(autofeeder)
endif()
