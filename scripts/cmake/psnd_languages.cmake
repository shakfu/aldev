# psnd_languages.cmake - Auto-discovery and registration for psnd languages
#
# This module provides functions to:
# 1. Discover language directories under source/langs/
# 2. Register language sources and include directories
# 3. Generate lang_config_generated.h and lang_dispatch_generated.h
#
# To add a new language, create source/langs/<name>/CMakeLists.txt with:
#   psnd_register_language(
#       NAME <name>
#       DISPLAY_NAME "Display Name"
#       DESCRIPTION "Short description"
#       COMMANDS cmd1 cmd2
#       EXTENSIONS .ext1 .ext2
#       SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/impl/file.c ...
#       INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
#   )

include_guard(GLOBAL)

# Global properties to collect language data across subdirectories
set_property(GLOBAL PROPERTY PSND_ALL_LANGUAGES "")
set_property(GLOBAL PROPERTY PSND_LANG_CONFIG_CONTENT "")
set_property(GLOBAL PROPERTY PSND_LANG_DISPATCH_CONTENT "")

# ==============================================================================
# psnd_discover_languages_in(langs_dir)
# Discover all language directories under the given path and process their CMakeLists.txt
# ==============================================================================
function(psnd_discover_languages_in langs_dir)
    set(PSND_LANGUAGES "")

    if(EXISTS "${langs_dir}")
        file(GLOB lang_dirs "${langs_dir}/*")

        foreach(lang_dir ${lang_dirs})
            if(IS_DIRECTORY ${lang_dir} AND EXISTS "${lang_dir}/CMakeLists.txt")
                get_filename_component(lang_name ${lang_dir} NAME)
                string(TOUPPER ${lang_name} LANG_UPPER)

                # Create option for each discovered language (default: ON)
                option(LANG_${LANG_UPPER} "Enable ${lang_name} language" ON)

                if(LANG_${LANG_UPPER})
                    message(STATUS "Discovered language: ${lang_name}")
                    list(APPEND PSND_LANGUAGES ${lang_name})

                    # Process the language's CMakeLists.txt
                    add_subdirectory(${lang_dir} ${CMAKE_BINARY_DIR}/langs/${lang_name})
                endif()
            endif()
        endforeach()
    endif()

    # Store in global property
    set_property(GLOBAL PROPERTY PSND_ALL_LANGUAGES "${PSND_LANGUAGES}")

    # Make available to parent scope
    set(PSND_LANGUAGES ${PSND_LANGUAGES} PARENT_SCOPE)
endfunction()

# ==============================================================================
# psnd_register_language()
# Called from each language's CMakeLists.txt to register its components
# ==============================================================================
function(psnd_register_language)
    cmake_parse_arguments(LANG
        ""                                           # Options (flags)
        "NAME;DISPLAY_NAME;DESCRIPTION"              # Single-value args
        "SOURCES;COMMANDS;EXTENSIONS;INCLUDE_DIRS;REPL_SOURCES;REGISTER_SOURCES;LINK_LIBRARIES"   # Multi-value args
        ${ARGN}
    )

    # Validate required arguments
    if(NOT LANG_NAME)
        message(FATAL_ERROR "psnd_register_language: NAME is required")
    endif()
    # SOURCES can be empty for languages that use thirdparty libraries directly
    # (e.g., TR7 uses the tr7 Scheme interpreter from thirdparty)

    # Defaults
    if(NOT LANG_DISPLAY_NAME)
        string(SUBSTRING ${LANG_NAME} 0 1 first_char)
        string(TOUPPER ${first_char} first_char_upper)
        string(SUBSTRING ${LANG_NAME} 1 -1 rest)
        set(LANG_DISPLAY_NAME "${first_char_upper}${rest}")
    endif()
    if(NOT LANG_DESCRIPTION)
        set(LANG_DESCRIPTION "${LANG_DISPLAY_NAME} language")
    endif()
    if(NOT LANG_COMMANDS)
        set(LANG_COMMANDS ${LANG_NAME})
    endif()
    if(NOT LANG_EXTENSIONS)
        set(LANG_EXTENSIONS ".${LANG_NAME}")
    endif()

    string(TOUPPER ${LANG_NAME} LANG_UPPER)

    # Store all properties in global scope for later collection
    set_property(GLOBAL PROPERTY PSND_LANG_${LANG_UPPER}_SOURCES "${LANG_SOURCES}")
    set_property(GLOBAL PROPERTY PSND_LANG_${LANG_UPPER}_INCLUDES "${LANG_INCLUDE_DIRS}")
    set_property(GLOBAL PROPERTY PSND_LANG_${LANG_UPPER}_DISPLAY "${LANG_DISPLAY_NAME}")
    set_property(GLOBAL PROPERTY PSND_LANG_${LANG_UPPER}_DESC "${LANG_DESCRIPTION}")
    set_property(GLOBAL PROPERTY PSND_LANG_${LANG_UPPER}_COMMANDS "${LANG_COMMANDS}")
    set_property(GLOBAL PROPERTY PSND_LANG_${LANG_UPPER}_EXTENSIONS "${LANG_EXTENSIONS}")
    set_property(GLOBAL PROPERTY PSND_LANG_${LANG_UPPER}_REPL_SOURCES "${LANG_REPL_SOURCES}")
    set_property(GLOBAL PROPERTY PSND_LANG_${LANG_UPPER}_REGISTER_SOURCES "${LANG_REGISTER_SOURCES}")
    set_property(GLOBAL PROPERTY PSND_LANG_${LANG_UPPER}_LINK_LIBRARIES "${LANG_LINK_LIBRARIES}")

    message(STATUS "  Registered: ${LANG_NAME} (${LANG_DISPLAY_NAME})")
endfunction()

# ==============================================================================
# psnd_generate_lang_headers()
# Generate lang_config_generated.h and lang_dispatch_generated.h
# ==============================================================================
function(psnd_generate_lang_headers)
    get_property(PSND_LANGUAGES GLOBAL PROPERTY PSND_ALL_LANGUAGES)

    # ============================================
    # Generate lang_config_generated.h
    # ============================================
    set(config_content "/* Auto-generated by CMake - DO NOT EDIT */\n")
    string(APPEND config_content "#ifndef LANG_CONFIG_GENERATED_H\n")
    string(APPEND config_content "#define LANG_CONFIG_GENERATED_H\n\n")

    # Helper macros for each language
    string(APPEND config_content "/* ======================= Helper Macros ==================================== */\n\n")
    foreach(lang ${PSND_LANGUAGES})
        string(TOUPPER ${lang} LANG_UPPER)
        string(APPEND config_content "#ifdef LANG_${LANG_UPPER}\n")
        string(APPEND config_content "#define IF_LANG_${LANG_UPPER}(x) x\n")
        string(APPEND config_content "#else\n")
        string(APPEND config_content "#define IF_LANG_${LANG_UPPER}(x)\n")
        string(APPEND config_content "#endif\n\n")
    endforeach()

    # Forward declarations
    string(APPEND config_content "/* ======================= Forward Declarations ============================= */\n\n")
    foreach(lang ${PSND_LANGUAGES})
        string(TOUPPER ${lang} LANG_UPPER)
        # Create proper title case for struct name
        string(SUBSTRING ${lang} 0 1 first_char)
        string(TOUPPER ${first_char} first_char_upper)
        string(SUBSTRING ${lang} 1 -1 rest)
        set(lang_title "${first_char_upper}${rest}")
        string(APPEND config_content "IF_LANG_${LANG_UPPER}(struct Loki${lang_title}State;)\n")
    endforeach()

    # State fields macro
    string(APPEND config_content "\n/* ======================= EditorModel State Fields ========================= */\n\n")
    string(APPEND config_content "#define LOKI_LANG_STATE_FIELDS_GENERATED \\\n")
    set(first_field TRUE)
    foreach(lang ${PSND_LANGUAGES})
        string(TOUPPER ${lang} LANG_UPPER)
        string(SUBSTRING ${lang} 0 1 first_char)
        string(TOUPPER ${first_char} first_char_upper)
        string(SUBSTRING ${lang} 1 -1 rest)
        set(lang_title "${first_char_upper}${rest}")
        string(APPEND config_content "    IF_LANG_${LANG_UPPER}(struct Loki${lang_title}State *${lang}_state;)")
        # Add backslash-newline for all but last
        list(LENGTH PSND_LANGUAGES num_langs)
        list(FIND PSND_LANGUAGES ${lang} lang_index)
        math(EXPR last_index "${num_langs} - 1")
        if(NOT lang_index EQUAL last_index)
            string(APPEND config_content " \\\n")
        else()
            string(APPEND config_content "\n")
        endif()
    endforeach()

    # Init function declarations
    string(APPEND config_content "\n/* ======================= Language Init Declarations ======================= */\n\n")
    foreach(lang ${PSND_LANGUAGES})
        string(TOUPPER ${lang} LANG_UPPER)
        string(APPEND config_content "IF_LANG_${LANG_UPPER}(void ${lang}_loki_lang_init(void);)\n")
    endforeach()

    # Init calls macro
    string(APPEND config_content "\n/* ======================= Language Init Calls ============================== */\n\n")
    string(APPEND config_content "#define LOKI_LANG_INIT_ALL_GENERATED() \\\n")
    foreach(lang ${PSND_LANGUAGES})
        string(TOUPPER ${lang} LANG_UPPER)
        string(APPEND config_content "    IF_LANG_${LANG_UPPER}(${lang}_loki_lang_init();)")
        list(LENGTH PSND_LANGUAGES num_langs)
        list(FIND PSND_LANGUAGES ${lang} lang_index)
        math(EXPR last_index "${num_langs} - 1")
        if(NOT lang_index EQUAL last_index)
            string(APPEND config_content " \\\n")
        else()
            string(APPEND config_content "\n")
        endif()
    endforeach()

    string(APPEND config_content "\n#endif /* LANG_CONFIG_GENERATED_H */\n")

    # ============================================
    # Generate lang_dispatch_generated.h
    # ============================================
    set(dispatch_content "/* Auto-generated by CMake - DO NOT EDIT */\n")
    string(APPEND dispatch_content "#ifndef LANG_DISPATCH_GENERATED_H\n")
    string(APPEND dispatch_content "#define LANG_DISPATCH_GENERATED_H\n\n")

    # Dispatch init declarations
    string(APPEND dispatch_content "/* ======================= Dispatch Init Declarations ======================= */\n\n")
    foreach(lang ${PSND_LANGUAGES})
        string(TOUPPER ${lang} LANG_UPPER)
        string(APPEND dispatch_content "#ifdef LANG_${LANG_UPPER}\n")
        string(APPEND dispatch_content "void ${lang}_dispatch_init(void);\n")
        string(APPEND dispatch_content "#endif\n\n")
    endforeach()

    # Dispatch init calls macro
    string(APPEND dispatch_content "/* ======================= Dispatch Init Calls ============================== */\n\n")
    string(APPEND dispatch_content "#define LANG_DISPATCH_INIT_ALL_GENERATED() \\\n")
    foreach(lang ${PSND_LANGUAGES})
        string(TOUPPER ${lang} LANG_UPPER)
        string(APPEND dispatch_content "    IF_LANG_${LANG_UPPER}(${lang}_dispatch_init();)")
        list(LENGTH PSND_LANGUAGES num_langs)
        list(FIND PSND_LANGUAGES ${lang} lang_index)
        math(EXPR last_index "${num_langs} - 1")
        if(NOT lang_index EQUAL last_index)
            string(APPEND dispatch_content " \\\n")
        else()
            string(APPEND dispatch_content "\n")
        endif()
    endforeach()

    string(APPEND dispatch_content "\n#endif /* LANG_DISPATCH_GENERATED_H */\n")

    # Create generated directory and write files
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
    file(WRITE "${CMAKE_BINARY_DIR}/generated/lang_config_generated.h" "${config_content}")
    file(WRITE "${CMAKE_BINARY_DIR}/generated/lang_dispatch_generated.h" "${dispatch_content}")

    message(STATUS "Generated: ${CMAKE_BINARY_DIR}/generated/lang_config_generated.h")
    message(STATUS "Generated: ${CMAKE_BINARY_DIR}/generated/lang_dispatch_generated.h")
endfunction()

# ==============================================================================
# psnd_collect_lang_sources()
# Collect all language sources and includes for linking
# Returns: PSND_ALL_LANG_SOURCES, PSND_ALL_LANG_INCLUDES, PSND_ALL_LANG_REPL_SOURCES
# ==============================================================================
function(psnd_collect_lang_sources)
    get_property(PSND_LANGUAGES GLOBAL PROPERTY PSND_ALL_LANGUAGES)

    set(all_sources "")
    set(all_includes "")
    set(all_repl_sources "")
    set(all_register_sources "")
    set(all_link_libs "")

    foreach(lang ${PSND_LANGUAGES})
        string(TOUPPER ${lang} LANG_UPPER)

        get_property(lang_sources GLOBAL PROPERTY PSND_LANG_${LANG_UPPER}_SOURCES)
        get_property(lang_includes GLOBAL PROPERTY PSND_LANG_${LANG_UPPER}_INCLUDES)
        get_property(lang_repl GLOBAL PROPERTY PSND_LANG_${LANG_UPPER}_REPL_SOURCES)
        get_property(lang_register GLOBAL PROPERTY PSND_LANG_${LANG_UPPER}_REGISTER_SOURCES)
        get_property(lang_libs GLOBAL PROPERTY PSND_LANG_${LANG_UPPER}_LINK_LIBRARIES)

        list(APPEND all_sources ${lang_sources})
        list(APPEND all_includes ${lang_includes})
        list(APPEND all_repl_sources ${lang_repl})
        list(APPEND all_register_sources ${lang_register})
        list(APPEND all_link_libs ${lang_libs})
    endforeach()

    set(PSND_ALL_LANG_SOURCES ${all_sources} PARENT_SCOPE)
    set(PSND_ALL_LANG_INCLUDES ${all_includes} PARENT_SCOPE)
    set(PSND_ALL_LANG_REPL_SOURCES ${all_repl_sources} PARENT_SCOPE)
    set(PSND_ALL_LANG_REGISTER_SOURCES ${all_register_sources} PARENT_SCOPE)
    set(PSND_ALL_LANG_LINK_LIBRARIES ${all_link_libs} PARENT_SCOPE)
endfunction()

# ==============================================================================
# psnd_apply_lang_definitions(target)
# Apply LANG_XXX compile definitions to a target for all enabled languages
# ==============================================================================
function(psnd_apply_lang_definitions target)
    get_property(PSND_LANGUAGES GLOBAL PROPERTY PSND_ALL_LANGUAGES)

    foreach(lang ${PSND_LANGUAGES})
        string(TOUPPER ${lang} LANG_UPPER)
        target_compile_definitions(${target} PRIVATE LANG_${LANG_UPPER}=1)
    endforeach()

    # MHS-specific definitions
    if(ENABLE_MHS_INTEGRATION)
        # If compilation support is disabled, define MHS_NO_COMPILATION
        if(NOT MHS_ENABLE_COMPILATION)
            target_compile_definitions(${target} PRIVATE MHS_NO_COMPILATION)
        endif()
        # If using package mode (vs source mode), define MHS_USE_PKG
        if(PSND_MHS_USE_PKG)
            target_compile_definitions(${target} PRIVATE MHS_USE_PKG)
        endif()
    endif()
endfunction()
