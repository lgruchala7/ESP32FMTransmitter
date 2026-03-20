idf_component_register(SRCS "sh1106.c" "bt_app_av.c"
                            "bt_app_core.c"
                            "main.c"
                            "si4713.c"
                            "i2c_master_app.c"
                            "sh1106.c"
                    PRIV_REQUIRES esp_driver_gpio esp_driver_i2s esp_driver_i2c bt nvs_flash esp_ringbuf esp_driver_dac
                    INCLUDE_DIRS ".")
