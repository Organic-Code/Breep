#########################################################################################
#                                                                                       #
# Copyright (C) 2016 LAZARE Lucas (lucas@lazare.fr)                                     #
#                                                                                       #
# This software is provided 'as-is', WITHOUT ANY EXPRESS OR IMPLIED WARRANTY.           #
# In NO EVENT will the authors be held liable for any damages arising from the          #
# use of this software.                                                                 #
#                                                                                       #
# Permission is granted to anyone to use this software for any purpose,                 #
# including commercial applications, and to alter it and redistribute it freely,        #
# subject to the following restrictions:                                                #
#                                                                                       #
# 1. The origin of this software must not be misrepresented;                            #
# you must not claim that you wrote the original software.                              #
# If you use this software in a product, an acknowledgment                              #
# in the product documentation would be appreciated but is not required.                #
#                                                                                       #
# 2. Altered source versions must be plainly marked as such,                            #
# and must not be misrepresented as being the original software.                        #
#                                                                                       #
# 3. This notice may not be removed or altered from any source distribution.            #
#                                                                                       #
#########################################################################################

# Notice : this file is NOT part of the Breep project

################################ User defined variables #################################

#Set BUILDINGLIB to 1 if the final product is a library rather than an executable
#Used for checking when the 'run' rule is invoked
BUILDINGLIB     = 0
#Set to 1 if you don't want make to link objects file together.
INHIBITLINKING  = 0

LEAKCHECKER = gdb

#Compiler command
COMPILER        = g++
#compiler flags
COMPFLAGS       =
#Compile standard
COMPSTANDARD    = -std=c++14
#Includes' flags
INCLUDEDIR      = -I ../../include
#Libraries paths' flags
LIBSDIR         = -L ../../bin
#Libraries links' flags
LINKS           = -lboost_system -pthread
#Extra compiler flags
DEFINES         =
#Compiler argument to name the output file
COMPOUTPUTNAME  = -o
#Compiler argument to generate an object file rather than directly an executable
COMPSTOP2OBJECT = -c
#Compiler argument for a debug build
COMPDEBUG       = -g
#Compiler argument added when mixing all objects files for the final product (ie: when building a library using GCC, add '-shared' here)
COMPFINALIZE    =
#Extensions for source files, space separated, without the '.' character
FILEIDENTIFIERS = cpp
#Files to exclude, space separated. Any file with that name will be ignored
EXCLUDEDEFILES  =
#Files to exclude, space separated. The full path from the Makefile directory is required, without './'
EXCLUDEDSPEC    =

#--Directories should be empty or end by '/'--
#Directory for the final executable
BUILDDIR        = ./build/
#Directory for objects file
OBJDIR          = ./build/
#Sources' directories, separated by a space
SOURCEDIR       = ./

#Name of the output executable
OUTNAME         = chat.elf
#Command to be run before anything is compiled
PREBUILDHOOK    =
#Command to be run just before linking obj files together, given all objects files as parameter
PRELINKHOOK     =
#Command to be run after a build is completed, given the final binary as parameter
POSTBUILDHOOK   =

#Message to display when calling PREBUILDHOOK
PREBUILDMSG     = "Building $(OUTNAME)"
#Message to display when calling PRELINKHOOK
PRELINKMSG      =
#Message to display when calling POSTBUILDHOOK
POSTBUILDMSG    =
#Command used when invoking 'make run'.
RUNCMD          = ./$(BUILDDIR)/$(OUTNAME)

#Misc
DISPLAY         = printf
MKDIR           = mkdir -p
RMDIR           = rmdir -p
RM              = rm -f
VOIDERROR       = 2>/dev/null
VOIDECHO        = >/dev/null 2>&1
#Displayed when the program is started using 'make run'
st              = 'STARTING'
#Display when the program exits and was started using 'make run'
eop             = 'END OF PROGRAM'

RED             = "\\033[0m\\033[1m\\033[91m"
GREEN           = "\\033[0m\\033[1m\\033[92m"
EMPH            = "\\033[0m\\033[33m"
RESET           = "\\033[0m"

#You should probably let the trailing
PRE             =[ $(RED)--$(RESET) ] #
POST            =\r[ $(GREEN)OK$(RESET) ]

################################## End of user defined ##################################


define uniq
  $(eval seen :=)
  $(foreach _,$1,$(if $(filter $_,${seen}),,$(eval seen += $_)))
  ${seen}
endef


RWILDCARD = $(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call RWILDCARD,$d/,$2))

OUTFINAL       := $(BUILDDIR)$(OUTNAME)
SOURCESNAME    := $(call uniq,$(foreach srcdir,$(SOURCEDIR),$(filter-out $(EXCLUDEDEFILES),$(notdir $(foreach fileid, $(FILEIDENTIFIERS),$(call RWILDCARD,$(srcdir),*.$(fileid)))))))
SOURCES        := $(filter-out $(abspath $(EXCLUDEDSPEC)),$(abspath $(foreach name,$(SOURCESNAME),$(foreach srcdir,$(SOURCEDIR),$(call RWILDCARD,$(srcdir),$(name))))))
SOURCES        := $(SOURCES:$(CURDIR)/%=%)
OBJECTS        := $(foreach src,$(SOURCES),$(OBJDIR)$(basename $(src)).o)
VPATH          := $(SOURCEDIR)
COMPFLAGS      += $(COMPSTANDARD)

CHARACTERS := A B C D E F G H I J K L M N O P Q R S T U V W X Y Z \
              a b c d e f g h i j k l m n o p q r s t u v w x y z \
              0 1 2 3 4 5 6 7 8 9 / \\ - _ . + \  [ ] { }

STRLEN = $(strip $(eval __temp := $(subst $(sp),x,$1))$(foreach a,$(CHARACTERS),$(eval __temp := $$(subst $a,x,$(__temp))))$(eval __temp := $(subst x,x ,$(__temp)))$(words $(__temp)))

TARGETSTOBUILD := $(words $(OBJECTS))
BUILTSOFAR     := 1

define draw_half_line
	for i in `seq 1 $$(($$(tput cols) / 2 - $(1))) $(VOIDERROR) || true`; do $(DISPLAY) '—'; done
endef

define make_line
	$(call draw_half_line, $$((($(call STRLEN, $(1)) + $(words $(1))) / 2)))
	$(DISPLAY) "$(1) "
	$(call draw_half_line, $$((($(call STRLEN, $(1)) + $(words $(1) + 1)) / 2)))
	$(DISPLAY) "\n\n"
endef

.SUFFIXES: $(foreach fileid,$(FILEIDENTIFIERS),.$(fileid))

.PHONY: all
all:
ifeq ($(strip $(OUTNAME)),)
	@$(DISPLAY) "OUTNAME variable not set !\n"
	@exit 1
endif
	@$(DISPLAY) "\n\n"
ifneq ($(strip $(PREBUILDMSG)),)
	@$(DISPLAY) $(PREBUILDMSG)
endif
	@$(PREBUILDHOOK)
	@$(DISPLAY) "\n"
	@$(MAKE) --silent $(OUTFINAL)

.PHONY: run
run:
ifeq ($(strip $(OUTNAME)),)
	@$(DISPLAY) "OUTNAME variable not set !\n"
	@exit 1
endif
ifeq ($(BUILDINGLIB),1)
	@$(DISPLAY) "ERROR: program built by this makefile isn't an executable\n"
	@exit 1
endif
	@$(DISPLAY) "\n"
ifneq ($(strip $(PREBUILDMSG)),)
	@$(DISPLAY) $(PREBUILDMSG)
endif
	@$(PREBUILDHOOK)
	@$(MAKE) --silent $(OUTFINAL)
	@$(call make_line, "$(st)")
	@$(DISPLAY) "\n"
	@$(RUNCMD)
	@$(DISPLAY) "\n\n"
	@$(call make_line, "$(eop)")
	@$(DISPLAY) "\n"

$(OUTFINAL): $(OBJECTS)
	@$(DISPLAY) "\n"
ifneq ($(strip $(PRELINKMSG)),)
	@$(DISPLAY) $(PRELINKMSG)
endif
ifneq ($(strip $(PRELINKHOOK)),)
	$(PRELINKHOOK) $(abspath $(OBJECTS))
endif
ifeq ($(INHIBITLINKING),0)
	@$(DISPLAY) "\n$(PRE)Building $(EMPH)$(OUTFINAL)$(RESET) from object files..."
ifeq ($(wildcard $(BUILDDIR)/.),)
	@$(MKDIR) $(BUILDDIR)
endif
	$(COMPILER) $(LIBSDIR) $(COMPOUTPUTNAME) $(OUTFINAL) $(COMPFINALIZE) $(OBJECTS) $(LINKS) 
	@$(DISPLAY) "$(POST)\n\n"
endif
ifneq ($(strip $(POSTBUILDMSG)),)
	@$(DISPLAY) $(POSTBUILDMSG)
endif
ifneq ($(strip $(POSTBUILDHOOK)),)
	$(POSTBUILDHOOK) $(abspath $(OUTFINAL))
endif
	@$(DISPLAY) "\n"

define objectify_rule
$$(OBJDIR)%.o: %.$1
	@$$(DISPLAY) "$$(PRE)($$(BUILTSOFAR)/$$(TARGETSTOBUILD)) Building $$(EMPH)$$(notdir $$@)$$(RESET) from $$(EMPH)$$(notdir $$^)$$(RESET)..."
	@$$(MKDIR) $$(OBJDIR)$$(dir $$^)
	$$(COMPILER) $$(COMPFLAGS) $$(INCLUDEDIR) $$(DEFINES) $$(COMPSTOP2OBJECT) $$^ $$(COMPOUTPUTNAME) $$@
	@$$(DISPLAY) "$$(POST)\n"
	@$$(eval BUILTSOFAR=$$(shell echo $$$$(($$(BUILTSOFAR) + 1))))
endef
$(foreach fileid,$(FILEIDENTIFIERS),$(eval $(call objectify_rule,$(fileid)))) #The power of templates

.PHONY: debug
debug:
	@$(MAKE) -B --silent _debug

.PHONY: _debug
_debug: COMPFLAGS = $(COMPDEBUG) $(COMPSTANDARD)
_debug: $(OUTFINAL)
ifeq ($(strip $(BUILDDIR)),)
	@$(LEAKCHECKER) ./$(OUTFINAL)
else
	@$(LEAKCHECKER) $(OUTFINAL)
endif

.PHONY: clean
clean:
	@$(DISPLAY) "$(RESET)Cleaning files and folders...\n"
	@$(foreach file,$(OBJECTS), test -e $(file) && $(DISPLAY) "\n$(PRE)Removing $(EMPH)$(file)$(RESET)..." && $(RM) $(file) && $(DISPLAY) "$(POST)";) true
	@$(foreach file,$(OUTFINAL), test -e $(file) && $(DISPLAY) "\n$(PRE)Removing $(EMPH)$(file)$(RESET)..." && $(RM) $(file) && $(DISPLAY) "$(POST)";) true
	@$(DISPLAY) "\n\nDeleting empty directories..."
	@$(RMDIR) $(dir $(OBJECTS) $(OUTFINAL)) $(BUILDDIR) $(OBJDIR) $(VOIDECHO) || true
	@$(DISPLAY) "\nDone\n"

.PHONY: list_include_dirs
list_include_dirs:
	@$(DISPLAY) "$(realpath $(INCLUDEDIR))\n"

.PHONY: list_src_dirs
list_src_dirs:
	@$(DISPLAY) "$(realpath $(SOURCEDIR))\n"

.PHONY: list_src_files
list_src_files:
	@$(DISPLAY) "$(realpath $(SOURCES))\n"


.PHONY: human-list_include_dirs
human-list_include_dirs: INCLUDEDIR := $(realpath $(INCLUDEDIR))
human-list_include_dirs:
	@$(DISPLAY) " $(foreach folder,$(INCLUDEDIR),$(folder:$(CURDIR)/%=%)/\n)"

.PHONY: human-list_src_dirs
human-list_src_dirs: SOURCEDIR := $(realpath $(SOURCEDIR))
human-list_src_dirs:
	@$(DISPLAY) " $(foreach folder,$(SOURCEDIR),$(folder:$(CURDIR)/%=%)\n)"

.PHONY: human-list_src_files
human-list_src_files:
	@$(DISPLAY) " $(foreach file,$(SOURCES),$(file)\n)"
