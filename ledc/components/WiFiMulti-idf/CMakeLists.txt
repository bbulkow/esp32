cmake_minimum_required(VERSION 3.5)

set(srcs
		"WiFiMulti.c"
		)

# everything needs the ESP32 flag, not sure why this won't work
# going to hack by adding the ESP32 define in the h file
	#`target_compile_options(${COMPONENT_LIB} PRIVATE "-DESP32")

	idf_component_register(SRCS "WiFiMulti-idf.c"
			INCLUDE_DIRS "./include"  )
