add_library(binlogfilter SHARED binlogfilter.cc binlogfiltersession.cc binlogconfig.cc)
target_link_libraries(binlogfilter maxscale-common)
set_target_properties(binlogfilter PROPERTIES VERSION "1.0.0" LINK_FLAGS -Wl,-z,defs)
install_module(binlogfilter core)
