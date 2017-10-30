cmake_minimum_required(VERSION 3.5)

set(PRJ bitraf_hydroponics)
project(${PRJ})

set(KicadUtilsDir ${CMAKE_CURRENT_LIST_DIR}/../thirdparty/kicad-utils)
set(KicadUtilsPyDir ${KicadUtilsDir}/py)
file(GLOB KICAD_UTILS_CMAKES ${KicadUtilsDir}/cmake/kicad_*.cmake)
foreach(X ${KICAD_UTILS_CMAKES})
  include(${X})
endforeach()

# Python and virtualenv setup
set(PY "${CMAKE_CURRENT_LIST_DIR}/env/bin/python")
set(PIP "${CMAKE_CURRENT_LIST_DIR}/env/bin/pip")
set(PIP_COOKIE "${CMAKE_CURRENT_LIST_DIR}/env/.pip-cookie")
set(VENV "${CMAKE_CURRENT_LIST_DIR}/env")
set(TOOLS "${CMAKE_CURRENT_LIST_DIR}/py/tools")

add_custom_target("virtualenv" DEPENDS "${PY}")
add_custom_target("pip-install"
    COMMENT "pip install -r requirements.txt"
    COMMAND ${PIP} install -r requirements.txt
    DEPENDS "${PY}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})

add_custom_command(OUTPUT "${PY}"
    COMMAND "virtualenv" "-ppython3" "env"
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})

add_custom_command(OUTPUT "${PIP_COOKIE}"
    COMMENT "pip install -r requirements.txt"
    COMMAND "${PIP}" "install" "-r" "${CMAKE_CURRENT_LIST_DIR}/requirements.txt"
    COMMAND ${CMAKE_COMMAND} -E touch "${PIP_COOKIE}"
    DEPENDS "${PY}")

# BOM XML file
add_custom_command(OUTPUT "${CMAKE_CURRENT_LIST_DIR}/${PRJ}.xml"
    COMMAND "${VENV}/bin/ee" "kicad-make-bom"
        "--pretty"
        "--sch" "${PRJ}.sch"
        "--out" "${PRJ}.xml"
    MAIN_DEPENDENCY "${CMAKE_CURRENT_LIST_DIR}/${PRJ}.sch"
    DEPENDS "${PIP_COOKIE}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})

add_custom_command(OUTPUT "${CMAKE_CURRENT_LIST_DIR}/${PRJ}-checks.md"
    COMMAND "${PY}" "${TOOLS}" --cmd run-checks
        --sch "${CMAKE_CURRENT_LIST_DIR}/${PRJ}.sch"
        --digikey-repo "${CMAKE_CURRENT_LIST_DIR}/digikey"
        > "${CMAKE_CURRENT_LIST_DIR}/${PRJ}-checks.md"
    MAIN_DEPENDENCY "${CMAKE_CURRENT_LIST_DIR}/${PRJ}.sch"
    DEPENDS "${PIP_COOKIE}" "${PY}" "${TOOLS}" "digikey-download-facts.cookie")

add_custom_command(OUTPUT "digikey-download-facts.cookie"
    COMMENT "Downloading facts from Digi-Key"
    COMMAND "${VENV}/bin/ee" "digikey-download-facts"
        --sch "${CMAKE_CURRENT_LIST_DIR}/${PRJ}.sch"
        --out "${CMAKE_CURRENT_LIST_DIR}/digikey"
    COMMAND "${CMAKE_COMMAND}" -E touch "digikey-download-facts.cookie"
    MAIN_DEPENDENCY "${CMAKE_CURRENT_LIST_DIR}/${PRJ}.sch"
    DEPENDS "${PIP_COOKIE}")

#
# Files for production
#

# Gerber
kicad_gerber(
    TARGET ${PRJ}-gerber
    PCB_FILE ${PRJ}.kicad_pcb
    DIR production/gerber
    NAMING_STYLE PROTEL
    ZIP_FILE production/gerber.zip)

# Pick and place files
add_custom_command(
    OUTPUT
        production/${PRJ}-top.pos
        production/${PRJ}-bottom.pos
    COMMAND ${CMAKE_COMMAND} -E echo "The Pick and place files production/${PRJ}-{top,bottom}.pos are outdated. Export the files from KiCAD to update them."
    COMMAND false
    MAIN_DEPENDENCY ${CMAKE_CURRENT_LIST_DIR}/${PRJ}.kicad_pcb)

# PnP files adjusted for EuroCircuits
add_custom_command(
    OUTPUT production/${PRJ}-eurocircuits.pos
    COMMAND ${CMAKE_COMMAND} -E make_directory production
    COMMAND "${PY}" ${CMAKE_CURRENT_LIST_DIR}/py/eurocircuits_pos_to_cpl.py
            --top production/${PRJ}-top.pos
            --bottom production/${PRJ}-bottom.pos
            --output production/${PRJ}-eurocircuits.pos
    DEPENDS ${PY} ${CMAKE_CURRENT_LIST_DIR}/py/eurocircuits_pos_to_cpl.py
            production/${PRJ}-top.pos
            production/${PRJ}-bottom.pos)

add_custom_command(OUTPUT "production/${PRJ}-eurocircuits-bom.csv"
    COMMAND ${CMAKE_COMMAND} -E make_directory production
    COMMAND "${PY}" "${TOOLS}" --cmd eurocircuits-bom
        --sch "${CMAKE_CURRENT_LIST_DIR}/${PRJ}.sch"
        --digikey-repo "${CMAKE_CURRENT_LIST_DIR}/digikey"
        > "production/${PRJ}-eurocircuits-bom.csv"
    MAIN_DEPENDENCY "${CMAKE_CURRENT_LIST_DIR}/${PRJ}.sch"
    DEPENDS "${PIP_COOKIE}" "${PY}" "${TOOLS}" "digikey-download-facts.cookie")

add_custom_command(OUTPUT production/${PRJ}-digikey-bom.csv
    COMMAND ${CMAKE_COMMAND} -E make_directory production
    COMMAND "${PY}" "${TOOLS}" --cmd digikey-bom
      --sch "${CMAKE_CURRENT_LIST_DIR}/${PRJ}.sch"
      --digikey-repo "${CMAKE_CURRENT_LIST_DIR}/digikey"
      > production/${PRJ}-digikey-bom.csv
    MAIN_DEPENDENCY "${CMAKE_CURRENT_LIST_DIR}/${PRJ}.sch"
    DEPENDS "${PIP_COOKIE}" "${PY}" "${TOOLS}" "digikey-download-facts.cookie")

add_custom_command(OUTPUT production/${PRJ}-digikey-bom-20.csv
    COMMAND ${CMAKE_COMMAND} -E make_directory production
    COMMAND "${PY}" "${TOOLS}" --cmd digikey-bom
      --sch "${CMAKE_CURRENT_LIST_DIR}/${PRJ}.sch"
      --digikey-repo "${CMAKE_CURRENT_LIST_DIR}/digikey"
      --quantity 20
      > production/${PRJ}-digikey-bom-20.csv
    MAIN_DEPENDENCY "${CMAKE_CURRENT_LIST_DIR}/${PRJ}.sch"
    DEPENDS "${PIP_COOKIE}" "${PY}" "${TOOLS}" "digikey-download-facts.cookie")

message(ENABLE_PNP=${ENABLE_PNP})
if (ENABLE_PNP)
  set(extra_deps production/${PRJ}-eurocircuits.pos)
endif ()

add_custom_target(${PRJ}-production ALL DEPENDS
		${PRJ}-gerber
    production/${PRJ}-eurocircuits-bom.csv
    production/${PRJ}-digikey-bom.csv
    production/${PRJ}-digikey-bom-20.csv
    ${extra_deps}
)
add_custom_target(${PRJ}-bom ALL DEPENDS "${PRJ}.xml")
add_custom_target(${PRJ}-checks ALL DEPENDS "${PRJ}-checks.md")
add_custom_target(${PRJ} ALL DEPENDS
		${PRJ}-checks
		${PRJ}-bom
    )
