MACRO(gossamer_manpage _section _basename)
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/man/man${_section})
    set(_output_man_file ${CMAKE_BINARY_DIR}/man/man${_section}/${_basename}.${_section})
    set(_output_pdf_file ${CMAKE_BINARY_DIR}/docs/${_basename}.pdf)
    set(_tex_file ${_basename}.tex)
    set(_source_file ${CMAKE_SOURCE_DIR}/docs/${_basename}.md)
    set(_install_dir "share/man/man{$_section}")
    add_custom_command(OUTPUT ${_output_man_file}
	    COMMAND ${PANDOC_EXECUTABLE}
            -s -t man
            -o ${_output_man_file}
            ${_source_file}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Building manpage ${_basename}.${_section}"
        VERBATIM)
#add_custom_command(OUTPUT ${_output_pdf_file}
    add_custom_command(OUTPUT ${_tex_file}
	    COMMAND ${PANDOC_EXECUTABLE}
            -t latex
	    --smart
	    --self-contained
	    --toc
	    --listings
            -o ${_tex_file}
            ${_source_file}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
	#COMMENT "Building PDF ${_output_pdf_file}"
	COMMENT "Building TEX ${_tex_file}"
        VERBATIM)

#set(ALL_DOCS "${ALL_DOCS};${_output_pdf_file}" PARENT_SCOPE)
    set(ALL_DOCS "${ALL_DOCS};${_tex_file}" PARENT_SCOPE)
    set(ALL_MAN_PAGES "${ALL_MAN_PAGES};${_output_man_file}" PARENT_SCOPE)

ENDMACRO(gossamer_manpage)


if(PANDOC_EXECUTABLE)
    message(STATUS "Looking for pandoc - found")
    message(STATUS "  Manual pages will be built")

    gossamer_manpage(1 gossple)
    gossamer_manpage(1 goss)
    gossamer_manpage(1 xenome)
    gossamer_manpage(1 electus)

    install(DIRECTORY ${CMAKE_BINARY_DIR}/docs DESTINATION share)

    add_custom_target(man ALL DEPENDS ${ALL_MAN_PAGES})
    add_custom_target(docs ALL DEPENDS ${ALL_DOCS})
else()
    message(STATUS "Looking for pandoc - not found")
    message(STATUS "  Manual pages will not be built")
endif(PANDOC_EXECUTABLE)


