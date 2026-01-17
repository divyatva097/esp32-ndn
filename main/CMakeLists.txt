idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "." "../components/ndn-lite"
                    REQUIRES ndn-lite nvs_flash esp_wifi esp_timer driver)