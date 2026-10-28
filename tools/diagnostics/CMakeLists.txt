qt_add_executable(metadata-network-probe MetadataNetworkProbe.cpp MetadataProbeInput.h)
target_link_libraries(metadata-network-probe PRIVATE YalahooCore Qt6::Network)
yalahoo_tool(metadata-network-probe)
