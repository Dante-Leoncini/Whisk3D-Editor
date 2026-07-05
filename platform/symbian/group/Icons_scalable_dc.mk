#
# ==============================================================================
#  Name        : Icons_scalable_dc.mk
#  Part of     : OpenGLEx / Whisk3D
# ==============================================================================
#


ifeq (WINS,$(findstring WINS, $(PLATFORM)))
ZDIR=$(EPOCROOT)epoc32\release\$(PLATFORM)\$(CFG)\Z
else
ZDIR=$(EPOCROOT)epoc32\data\z
endif

TARGETDIR=$(ZDIR)\resource\apps
ICONTARGETFILENAME=$(TARGETDIR)\whisk3D_icon.mif

ICONDIR=..\gfx

do_nothing :
	@rem do_nothing

MAKMAKE : do_nothing

BLD : do_nothing

CLEAN : do_nothing

LIB : do_nothing

CLEANLIB : do_nothing

#RESOURCE :
#	mifconv $(ICONTARGETFILENAME) /h$(HEADERFILENAME) /FIcons.miflist

# /V3: el mifconv del SDK de Symbian^3 convierte el SVG a NVG (formato que solo
# entienden telefonos Symbian^3) y el N95/S60v3-v5 muestra el icono roto.
# /V3 fuerza el SVGB clasico (RGB/fixed point), que entienden todos.
RESOURCE :
	mifconv $(ICONTARGETFILENAME) \
		/V3 /c32 $(ICONDIR)\qgn_menu_whisk3D.svg

FREEZE : do_nothing

SAVESPACE : do_nothing

RELEASABLES :
	@echo $(ICONTARGETFILENAME)

FINAL : do_nothing

