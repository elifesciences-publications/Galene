macro(target_apple_sdk sdk_ver)
   if (APPLE)
      set(TARGET_SDKROOT "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX${sdk_ver}.sdk")
      # check to see if SDK exists and use it if so
      if(EXISTS ${TARGET_SDKROOT})
      else()
         set(CMAKE_OSX_SYSROOT ${TARGET_SDKROOT})
         message(WARNING "Could not find macOS ${sdk_ver} SDK, falling back on default")
      endif()
   endif()
endmacro()
