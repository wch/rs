/*
 *  R : A Computer Language for Statistical Data Analysis
 *  Copyright (C) 1995, 1996  Robert Gentleman and Ross Ihaka
 *  Copyright (C) 1997--2001  Robert Gentleman, Ross Ihaka and the
 *			      R Development Core Team
 *  Copyright (C) 2002--2003  The R Foundation
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  A copy of the GNU General Public License is available via WWW at
 *  http://www.gnu.org/copyleft/gpl.html.  You can also obtain it by
 *  writing to the Free Software Foundation, Inc., 59 Temple Place,
 *  Suite 330, Boston, MA  02111-1307  USA.


 *  This is an extensive reworking by Paul Murrell of an original
 *  quick hack by Ross Ihaka designed to give a superset of the
 *  functionality in the AT&T Bell Laboratories GRZ library.
 *
 *  NOTE : ./plotmath.c	 has partly similar functionality for "math graphics"
	    ~~~~~~~~~~~
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <Defn.h>
#include <Graphics.h>
#include <Rdevices.h>		/* KillAllDevices */
#include <Rmath.h>		/* eg. fmax2() */
#include <R_ext/Applic.h>	/* pretty0() */

#include <string.h>
#include <stdlib.h>

extern int baseRegisterIndex;

static char HexDigits[] = "0123456789ABCDEF";


/*--->> Documentation now in  ../include/Rgraphics.h  "API" ----- */

/*-------------------------------------------------------------------
 *
 *  TRANSFORMATIONS
 *
 *    There are five major regions on a device, for any
 *    particular figure:  the outer margins, which "stick"
 *    to the edges of the device;  the inner region, which
 *    is defined as the total device less the outer margins;
 *    the figure region, which defaults from the current
 *    layout (mfrow, mfcol, layout) unless the user specifies
 *    it directly (fig, fin);  the figure margins, which
 *    "stick" to the edges of the plot region;	and thed
 *    plot region, which is the figure region less the figure
 *    margins by default unless the user specifies it directly
 *    (plt, pin)
 *
 *  COORDINATE SYSTEMS
 *
 *    DEVICE  = devices natural coordinate system
 *		(e.g., pixels, 1/72", ...)
 *    NDC     = normalised device coordinates (0..1 on device)
 *    INCHES  = inches
 *    OMA1..4 = outer margin coordinates
 *    NIC     = normalised inner region coordinates
 *		(0..1 on inner region)
 *    NFC     = normalised figure coordinates
 *		(0..1 on figure region)
 *    MAR1..4 = figure margin coordinates
 *    NPC     = normalised plot coordinates
 *		(0..1 on plot region)
 *    USER    = world or data coordinates
 *
 *
 *  UNITS
 *
 *    All of the above, except OMA1..4 and MAR1..4, plus ...
 *
 *	LINES = line coordinates (lines of margin;  based on mex)
 *	CHARS = char coordinates (lines of text;  based on cex)
 *
 *    The function Convert(value, from, to) is provided
 *    to transform between any pair of coordinate systems
 *    (for transforming locations)
 *
 *    The functions ConvertXUnits(value, from, to) and
 *    ConvertYUnits(value, from, to) are provided to transform
 *    between any pair of units (for transforming dimensions)
 *
 *    IMPORTANT:  if user coordinates are logged, then the
 *    conversion to/from USER units will not work.  in this
 *    case it is necessary to use convert(x1) - convert(x2)
 *    rather than convert(x1 - x2)
 *
 */

double R_Log10(double x)
{
    return (R_FINITE(x) && x > 0.0) ? log10(x) : NA_REAL;
}

/* In interpreted R, units are as follows:
 *	1 = "user"
 *	2 = "figure"
 *	3 = "inches"
 * the function GMapUnits provides a mapping
 * between interpreted units and internal units.
 */
GUnit GMapUnits(int Runits)
{
    switch (Runits) {
    case 1:	return USER;
    case 2:	return NFC;
    case 3:	return INCHES;
    default:	return 0;
    }
}

	/* Conversions Between Units*/

/* Used to be global (non-static) -- but are nowhere declared.
 * The public interface is through G[XY]ConvertUnits() */

static double xNDCtoDevUnits(double x, DevDesc *dd)
{
    return x*fabs(Rf_gpptr(dd)->ndc2dev.bx);
}

static double yNDCtoDevUnits(double y, DevDesc *dd)
{
    return y*fabs(Rf_gpptr(dd)->ndc2dev.by);
}

static double xNICtoDevUnits(double x, DevDesc *dd)
{
    return x*fabs(Rf_gpptr(dd)->inner2dev.bx);
}

static double yNICtoDevUnits(double y, DevDesc *dd)
{
    return y*fabs(Rf_gpptr(dd)->inner2dev.by);
}

static double xNFCtoDevUnits(double x, DevDesc *dd)
{
    return x*fabs(Rf_gpptr(dd)->fig2dev.bx);
}

static double yNFCtoDevUnits(double y, DevDesc *dd)
{
    return y*fabs(Rf_gpptr(dd)->fig2dev.by);
}

static double xNPCtoDevUnits(double x, DevDesc *dd)
{
    return xNFCtoDevUnits(x*(Rf_gpptr(dd)->plt[1] - Rf_gpptr(dd)->plt[0]), dd);
}

static double yNPCtoDevUnits(double y, DevDesc *dd)
{
    return yNFCtoDevUnits(y*(Rf_gpptr(dd)->plt[3] - Rf_gpptr(dd)->plt[2]), dd);
}

static double xUsrtoDevUnits(double x, DevDesc *dd)
{
    return xNFCtoDevUnits(x*Rf_gpptr(dd)->win2fig.bx, dd);
}

static double yUsrtoDevUnits(double y, DevDesc *dd)
{
    return yNFCtoDevUnits(y*Rf_gpptr(dd)->win2fig.by, dd);
}

static double xInchtoDevUnits(double x, DevDesc *dd)
{
    return xNDCtoDevUnits(x*Rf_gpptr(dd)->xNDCPerInch, dd);
}

static double yInchtoDevUnits(double y, DevDesc *dd)
{
    return yNDCtoDevUnits(y*Rf_gpptr(dd)->yNDCPerInch, dd);
}

static double xLinetoDevUnits(double x, DevDesc *dd)
{
    return xNDCtoDevUnits(x*Rf_gpptr(dd)->xNDCPerLine, dd);
}

static double yLinetoDevUnits(double y, DevDesc *dd)
{
    return yNDCtoDevUnits(y*Rf_gpptr(dd)->yNDCPerLine, dd);
}

static double xChartoDevUnits(double x, DevDesc *dd)
{
    return xNDCtoDevUnits(x*Rf_gpptr(dd)->cex*Rf_gpptr(dd)->xNDCPerChar, dd);
}

static double yChartoDevUnits(double y, DevDesc *dd)
{
    return yNDCtoDevUnits(y*Rf_gpptr(dd)->cex*Rf_gpptr(dd)->yNDCPerChar, dd);
}

static double xDevtoNDCUnits(double x, DevDesc *dd)
{
    return x/fabs(Rf_gpptr(dd)->ndc2dev.bx);
}

static double yDevtoNDCUnits(double y, DevDesc *dd)
{
    return y/fabs(Rf_gpptr(dd)->ndc2dev.by);
}

static double xDevtoNICUnits(double x, DevDesc *dd)
{
    return x/fabs(Rf_gpptr(dd)->inner2dev.bx);
}

static double yDevtoNICUnits(double y, DevDesc *dd)
{
    return y/fabs(Rf_gpptr(dd)->inner2dev.by);
}

static double xDevtoNFCUnits(double x, DevDesc *dd)
{
    return x/fabs(Rf_gpptr(dd)->fig2dev.bx);
}

static double yDevtoNFCUnits(double y, DevDesc *dd)
{
    return y/fabs(Rf_gpptr(dd)->fig2dev.by);
}

static double xDevtoNPCUnits(double x, DevDesc *dd)
{
    return xDevtoNFCUnits(x, dd)/(Rf_gpptr(dd)->plt[1] - Rf_gpptr(dd)->plt[0]);
}

static double yDevtoNPCUnits(double y, DevDesc *dd)
{
    return yDevtoNFCUnits(y, dd)/(Rf_gpptr(dd)->plt[3] - Rf_gpptr(dd)->plt[2]);
}

static double xDevtoUsrUnits(double x, DevDesc *dd)
{
    return xDevtoNFCUnits(x, dd)/Rf_gpptr(dd)->win2fig.bx;
}

static double yDevtoUsrUnits(double y, DevDesc *dd)
{
    return yDevtoNFCUnits(y, dd)/Rf_gpptr(dd)->win2fig.by;
}

static double xDevtoInchUnits(double x, DevDesc *dd)
{
    return xDevtoNDCUnits(x, dd)/Rf_gpptr(dd)->xNDCPerInch;
}

static double yDevtoInchUnits(double y, DevDesc *dd)
{
    return yDevtoNDCUnits(y, dd)/Rf_gpptr(dd)->yNDCPerInch;
}

static double xDevtoLineUnits(double x, DevDesc *dd)
{
    return xDevtoNDCUnits(x, dd)/Rf_gpptr(dd)->xNDCPerLine;
}

static double yDevtoLineUnits(double y, DevDesc *dd)
{
    return yDevtoNDCUnits(y, dd)/Rf_gpptr(dd)->yNDCPerLine;
}

/* NOTE that use the _current_ Rf_gpptr(dd)->cex here */
/* the conversion for lines doesn't have to worry about */
/* this because Rf_gpptr(dd)->mex can only be set once per plot */

static double xDevtoCharUnits(double x, DevDesc *dd)
{
    return xDevtoNDCUnits(x, dd)/(Rf_gpptr(dd)->cex * Rf_gpptr(dd)->xNDCPerChar);
}

static double yDevtoCharUnits(double y, DevDesc *dd)
{
    return yDevtoNDCUnits(y, dd)/(Rf_gpptr(dd)->cex * Rf_gpptr(dd)->yNDCPerChar);
}

static void BadUnitsError(char *where)
{
    error("Bad units specified in %s, please report!", where);
}

/* GConvertXUnits() and GConvertYUnits() convert
   a single value fromUnits toUnits : */

double GConvertXUnits(double x, GUnit fromUnits, GUnit toUnits, DevDesc *dd)
{
    double dev, final;
    switch (fromUnits) {
    case DEVICE: dev = x; break;
    case NDC:	 dev = xNDCtoDevUnits(x, dd); break;
    case NIC:	 dev = xNICtoDevUnits(x, dd); break;
    case NFC:	 dev = xNFCtoDevUnits(x, dd); break;
    case NPC:	 dev = xNPCtoDevUnits(x, dd); break;
    case USER:	 dev = xUsrtoDevUnits(x, dd); break;
    case INCHES: dev = xInchtoDevUnits(x, dd); break;
    case LINES:	 dev = xLinetoDevUnits(x, dd); break;
    case CHARS:	 dev = xChartoDevUnits(x, dd); break;
    default:	 dev = 0; BadUnitsError("GConvertXUnits");

    }
    switch (toUnits) {
    case DEVICE: final = dev; break;
    case NDC:	 final = xDevtoNDCUnits(dev, dd); break;
    case NIC:	 final = xDevtoNICUnits(dev, dd); break;
    case NFC:	 final = xDevtoNFCUnits(dev, dd); break;
    case NPC:	 final = xDevtoNPCUnits(dev, dd); break;
    case USER:	 final = xDevtoUsrUnits(dev, dd); break;
    case INCHES: final = xDevtoInchUnits(dev, dd); break;
    case LINES:	 final = xDevtoLineUnits(dev, dd); break;
    case CHARS:	 final = xDevtoCharUnits(dev, dd); break;
    default:	 final = 0; BadUnitsError("GConvertXUnits");
    }
    return final;
}

double GConvertYUnits(double y, GUnit fromUnits, GUnit toUnits, DevDesc *dd)
{
    double dev, final;
    switch (fromUnits) {
    case DEVICE: dev = y; break;
    case NDC:	 dev = yNDCtoDevUnits(y, dd); break;
    case NIC:	 dev = yNICtoDevUnits(y, dd); break;
    case NFC:	 dev = yNFCtoDevUnits(y, dd); break;
    case NPC:	 dev = yNPCtoDevUnits(y, dd); break;
    case USER:	 dev = yUsrtoDevUnits(y, dd); break;
    case INCHES: dev = yInchtoDevUnits(y, dd); break;
    case LINES:	 dev = yLinetoDevUnits(y, dd); break;
    case CHARS:	 dev = yChartoDevUnits(y, dd); break;
    default:	 dev = 0; BadUnitsError("GConvertYUnits");
    }
    switch (toUnits) {
    case DEVICE: final = dev; break;
    case NDC:	 final = yDevtoNDCUnits(dev, dd); break;
    case NIC:	 final = yDevtoNICUnits(dev, dd); break;
    case NFC:	 final = yDevtoNFCUnits(dev, dd); break;
    case NPC:	 final = yDevtoNPCUnits(dev, dd); break;
    case USER:	 final = yDevtoUsrUnits(dev, dd); break;
    case INCHES: final = yDevtoInchUnits(dev, dd); break;
    case LINES:	 final = yDevtoLineUnits(dev, dd); break;
    case CHARS:	 final = yDevtoCharUnits(dev, dd); break;
    default:	 final = 0; BadUnitsError("GConvertYUnits");
    }
    return final;
}

/* Functions to convert locations from one coordinate system to another */

/* OTHER coordinate systems to DEVICE */

/* Used to be global (non-static) -- but are nowhere declared.
 * The public interface is  GConvert(), GConvertX(), GConvertY() */
static double xNDCtoDev(double x, DevDesc *dd)
{
    return Rf_gpptr(dd)->ndc2dev.ax + x*Rf_gpptr(dd)->ndc2dev.bx;
}

static double yNDCtoDev(double y, DevDesc *dd)
{
    return Rf_gpptr(dd)->ndc2dev.ay + y*Rf_gpptr(dd)->ndc2dev.by;
}

static double xInchtoDev(double x, DevDesc *dd)
{
    return xNDCtoDev(x*Rf_gpptr(dd)->xNDCPerInch, dd);
}

static double yInchtoDev(double y, DevDesc *dd)
{
    return yNDCtoDev(y*Rf_gpptr(dd)->yNDCPerInch, dd);
}

static double xLinetoDev(double x, DevDesc *dd)
{
    return xNDCtoDev(x*Rf_gpptr(dd)->xNDCPerLine, dd);
}

static double yLinetoDev(double y, DevDesc *dd)
{
    return yNDCtoDev(y*Rf_gpptr(dd)->yNDCPerLine, dd);
}

static double xNICtoDev(double x, DevDesc *dd)
{
    return Rf_gpptr(dd)->inner2dev.ax + x*Rf_gpptr(dd)->inner2dev.bx;
}

static double yNICtoDev(double y, DevDesc *dd)
{
    return Rf_gpptr(dd)->inner2dev.ay + y*Rf_gpptr(dd)->inner2dev.by;
}
/* NOTE that an x-coordinate in OMA2 or OMA4 converts to a */
/* y-coordinate in Dev and a y-coordinate in OMA2 or OMA4 */
/* converts to an x-coordinate in Dev */

static double xOMA1toDev(double x, DevDesc *dd)
{
    return xNICtoDev(x, dd);
}

static double yOMA1toDev(double y, DevDesc *dd)
{
    return yLinetoDev((Rf_gpptr(dd)->oma[0] - y), dd);
}

static double xOMA2toyDev(double x, DevDesc *dd)
{
    return yNICtoDev(x, dd);
}

static double yOMA2toxDev(double y, DevDesc *dd)
{
    return xLinetoDev((Rf_gpptr(dd)->oma[1] - y), dd);
}

static double xOMA3toDev(double x, DevDesc *dd)
{
    return xNICtoDev(x, dd);
}

static double yOMA3toDev(double y, DevDesc *dd)
{
    double ndc = 1.0-yDevtoNDC(yLinetoDev((Rf_gpptr(dd)->oma[2] - y), dd), dd);
    return yNDCtoDev(ndc, dd);
}

static double xOMA4toyDev(double x, DevDesc *dd)
{
    return yNICtoDev(x, dd);
}

static double yOMA4toxDev(double y, DevDesc *dd)
{
    double ndc = 1.0-xDevtoNDC(xLinetoDev(Rf_gpptr(dd)->oma[3]-y, dd), dd);
    return xNDCtoDev(ndc, dd);
}

static double xNFCtoDev(double x, DevDesc *dd)
{
    return Rf_gpptr(dd)->fig2dev.ax + x*Rf_gpptr(dd)->fig2dev.bx;
}

static double yNFCtoDev(double y, DevDesc *dd)
{
    return Rf_gpptr(dd)->fig2dev.ay + y*Rf_gpptr(dd)->fig2dev.by;
}

static double xNPCtoDev(double x, DevDesc *dd)
{
    return xNFCtoDev(Rf_gpptr(dd)->plt[0] +
		     x*(Rf_gpptr(dd)->plt[1] - Rf_gpptr(dd)->plt[0]), dd);
}

static double yNPCtoDev(double y, DevDesc *dd)
{
    return yNFCtoDev(Rf_gpptr(dd)->plt[2] +
		     y*(Rf_gpptr(dd)->plt[3] - Rf_gpptr(dd)->plt[2]), dd);
}

static double xUsrtoDev(double x, DevDesc *dd)
{
    if (Rf_gpptr(dd)->xlog)
	x = R_Log10(x);
    return xNFCtoDev(Rf_gpptr(dd)->win2fig.ax + x*Rf_gpptr(dd)->win2fig.bx, dd);
}

static double yUsrtoDev(double y, DevDesc *dd)
{
    if (Rf_gpptr(dd)->ylog)
	y = R_Log10(y);
    return yNFCtoDev(Rf_gpptr(dd)->win2fig.ay + y*Rf_gpptr(dd)->win2fig.by, dd);
}

/* NOTE that an x-coordinate in MAR2 or MAR4 converts to a */
/* y-coordinate in Dev and a y-coordinate in MAR2 or MAR4 */
/* converts to an x-coordinate in Dev */

static double xMAR1toDev(double x, DevDesc *dd)
{
    return xUsrtoDev(x, dd);
}

static double yMAR1toDev(double y, DevDesc *dd)
{
    double nfc = GConvertYUnits(y, LINES, NFC, dd);
    return yNFCtoDev(Rf_gpptr(dd)->plt[2] - nfc, dd);
}

static double xMAR2toyDev(double x, DevDesc *dd)
{
    return yUsrtoDev(x, dd);
}

static double yMAR2toxDev(double y, DevDesc *dd)
{
    double nfc = GConvertXUnits(y, LINES, NFC, dd);
    return xNFCtoDev(Rf_gpptr(dd)->plt[0] - nfc, dd);
}

static double xMAR3toDev(double x, DevDesc *dd)
{
    return xUsrtoDev(x, dd);
}

static double yMAR3toDev(double y, DevDesc *dd)
{
    double nfc = GConvertYUnits(y, LINES, NFC, dd);
    return yNFCtoDev(Rf_gpptr(dd)->plt[3] + nfc, dd);
}

static double xMAR4toyDev(double x, DevDesc *dd)
{
    return yUsrtoDev(x, dd);
}

static double yMAR4toxDev(double y, DevDesc *dd)
{
    double nfc = GConvertXUnits(y, LINES, NFC, dd);
    return xNFCtoDev(Rf_gpptr(dd)->plt[1] + nfc, dd);
}

/* DEVICE coordinates to OTHER */

double xDevtoNDC(double x, DevDesc *dd)
{
    return (x - Rf_gpptr(dd)->ndc2dev.ax)/Rf_gpptr(dd)->ndc2dev.bx;
}

double yDevtoNDC(double y, DevDesc *dd)
{
    return (y - Rf_gpptr(dd)->ndc2dev.ay)/Rf_gpptr(dd)->ndc2dev.by;
}

static double xDevtoInch(double x, DevDesc *dd)
{
    return xDevtoNDC(x, dd)/Rf_gpptr(dd)->xNDCPerInch;
}

static double yDevtoInch(double y, DevDesc *dd)
{
    return yDevtoNDC(y, dd)/Rf_gpptr(dd)->yNDCPerInch;
}

static double xDevtoLine(double x, DevDesc *dd)
{
  return xDevtoNDC(x, dd)/Rf_gpptr(dd)->xNDCPerLine;
}

static double yDevtoLine(double y, DevDesc *dd)
{
    return yDevtoNDC(y, dd)/Rf_gpptr(dd)->yNDCPerLine;
}

static double xDevtoNIC(double x, DevDesc *dd)
{
    return (x - Rf_gpptr(dd)->inner2dev.ax)/Rf_gpptr(dd)->inner2dev.bx;
}

static double yDevtoNIC(double y, DevDesc *dd)
{
    return (y - Rf_gpptr(dd)->inner2dev.ay)/Rf_gpptr(dd)->inner2dev.by;
}

static double xDevtoOMA1(double x, DevDesc *dd)
{
    return xDevtoNIC(x, dd);
}

static double yDevtoOMA1(double y, DevDesc *dd)
{
    return Rf_gpptr(dd)->oma[0] - yDevtoLine(y, dd);
}

static double xDevtoyOMA2(double x, DevDesc *dd)
{
    return Rf_gpptr(dd)->oma[1] - xDevtoLine(x, dd);
}

static double yDevtoxOMA2(double y, DevDesc *dd)
{
    return yDevtoNIC(y, dd);
}

static double xDevtoOMA3(double x, DevDesc *dd)
{
    return xDevtoNIC(x, dd);
}

static double yDevtoOMA3(double y, DevDesc *dd)
{
    double line = (1.0 - yDevtoNDC(y, dd))/Rf_gpptr(dd)->yNDCPerLine;
    return Rf_gpptr(dd)->oma[2] - line;
}

static double xDevtoyOMA4(double x, DevDesc *dd)
{
    double line = (1.0 - xDevtoNDC(x, dd))/Rf_gpptr(dd)->xNDCPerLine;
    return Rf_gpptr(dd)->oma[3] - line;
}

static double yDevtoxOMA4(double y, DevDesc *dd)
{
    return yDevtoNIC(y, dd);
}

double xDevtoNFC(double x, DevDesc *dd)
{
    return (x - Rf_gpptr(dd)->fig2dev.ax)/Rf_gpptr(dd)->fig2dev.bx;
}

double yDevtoNFC(double y, DevDesc *dd)
{
    return (y - Rf_gpptr(dd)->fig2dev.ay)/Rf_gpptr(dd)->fig2dev.by;
}

double xDevtoNPC(double x, DevDesc *dd)
{
    return (xDevtoNFC(x, dd) - Rf_gpptr(dd)->plt[0])/
	(Rf_gpptr(dd)->plt[1] - Rf_gpptr(dd)->plt[0]);
}

double yDevtoNPC(double y, DevDesc *dd)
{
    return (yDevtoNFC(y, dd) - Rf_gpptr(dd)->plt[2])/
	(Rf_gpptr(dd)->plt[3] - Rf_gpptr(dd)->plt[2]);
}

/* a special case (NPC = normalised plot region coordinates) */

double xNPCtoUsr(double x, DevDesc *dd)
{
    if (Rf_gpptr(dd)->xlog)
	return pow(10., Rf_gpptr(dd)->logusr[0] +
		   x*(Rf_gpptr(dd)->logusr[1] - Rf_gpptr(dd)->logusr[0]));
    else
	return Rf_gpptr(dd)->usr[0] + x*(Rf_gpptr(dd)->usr[1] - Rf_gpptr(dd)->usr[0]);
}

double yNPCtoUsr(double y, DevDesc *dd)
{
    if (Rf_gpptr(dd)->ylog)
	return pow(10., Rf_gpptr(dd)->logusr[2] +
		   y*(Rf_gpptr(dd)->logusr[3]-Rf_gpptr(dd)->logusr[2]));
    else
	return Rf_gpptr(dd)->usr[2] + y*(Rf_gpptr(dd)->usr[3] - Rf_gpptr(dd)->usr[2]);
}

double xDevtoUsr(double x, DevDesc *dd)
{
    double nfc = xDevtoNFC(x, dd);
    if (Rf_gpptr(dd)->xlog)
	return pow(10., (nfc - Rf_gpptr(dd)->win2fig.ax)/Rf_gpptr(dd)->win2fig.bx);
    else
	return (nfc - Rf_gpptr(dd)->win2fig.ax)/Rf_gpptr(dd)->win2fig.bx;
}

double yDevtoUsr(double y, DevDesc *dd)
{
  double nfc = yDevtoNFC(y, dd);
  if (Rf_gpptr(dd)->ylog)
    return pow(10., (nfc - Rf_gpptr(dd)->win2fig.ay)/Rf_gpptr(dd)->win2fig.by);
  else
    return (nfc - Rf_gpptr(dd)->win2fig.ay)/Rf_gpptr(dd)->win2fig.by;
}

static double xDevtoMAR1(double x, DevDesc *dd)
{
    return xDevtoUsr(x, dd);
}

static double yDevtoMAR1(double y, DevDesc *dd)
{
    return Rf_gpptr(dd)->oma[0] + Rf_gpptr(dd)->mar[0] - yDevtoLine(y, dd);
}

static double xDevtoyMAR2(double x, DevDesc *dd)
{
    return Rf_gpptr(dd)->oma[1] + Rf_gpptr(dd)->mar[1] - xDevtoLine(x, dd);
}

static double yDevtoxMAR2(double y, DevDesc *dd)
{
    return yDevtoUsr(y, dd);
}

static double xDevtoMAR3(double x, DevDesc *dd)
{
    return xDevtoUsr(x, dd);
}

static double yDevtoMAR3(double y, DevDesc *dd)
{
    double line = GConvertYUnits(1.0 - yDevtoNFC(y, dd), NFC, LINES, dd);
    return Rf_gpptr(dd)->mar[2] - line;
}

static double xDevtoyMAR4(double x, DevDesc *dd)
{
    double line = GConvertXUnits(1.0 - xDevtoNFC(x, dd), NFC, LINES, dd);
    return Rf_gpptr(dd)->mar[3] - line;
}

static double yDevtoxMAR4(double y, DevDesc *dd)
{
    return yDevtoUsr(y, dd);
}

/* the Convert function converts a LOCATION in the FROM coordinate */
/* system to a LOCATION in the TO coordinate system */

void GConvert(double *x, double *y, GUnit from, GUnit to, DevDesc *dd)
{
    double devx, devy;

    switch (from) {
    case DEVICE:
	devx = *x;
	devy = *y;
	break;
    case NDC:
	devx = xNDCtoDev(*x, dd);
	devy = yNDCtoDev(*y, dd);
	break;
    case INCHES:
	devx = xInchtoDev(*x, dd);
	devy = yInchtoDev(*y, dd);
	break;
    case OMA1:
	devx = xOMA1toDev(*x, dd);
	devy = yOMA1toDev(*y, dd);
	break;
    case OMA2:
	devx = yOMA2toxDev(*y, dd);
	devy = xOMA2toyDev(*x, dd);
	break;
    case OMA3:
	devx = xOMA3toDev(*x, dd);
	devy = yOMA3toDev(*y, dd);
	break;
    case OMA4:
	devx = yOMA4toxDev(*y, dd);
	devy = xOMA4toyDev(*x, dd);
	break;
    case NIC:
	devx = xNICtoDev(*x, dd);
	devy = yNICtoDev(*y, dd);
	break;
    case NFC:
	devx = xNFCtoDev(*x, dd);
	devy = yNFCtoDev(*y, dd);
	break;
    case MAR1:
	devx = xMAR1toDev(*x, dd);
	devy = yMAR1toDev(*y, dd);
	break;
    case MAR2:
	devx = yMAR2toxDev(*y, dd);
	devy = xMAR2toyDev(*x, dd);
	break;
    case MAR3:
	devx = xMAR3toDev(*x, dd);
	devy = yMAR3toDev(*y, dd);
	break;
    case MAR4:
	devx = yMAR4toxDev(*y, dd);
	devy = xMAR4toyDev(*x, dd);
	break;
    case NPC:
	devx = xNPCtoDev(*x, dd);
	devy = yNPCtoDev(*y, dd);
	break;
    case USER:
	devx = xUsrtoDev(*x, dd);
	devy = yUsrtoDev(*y, dd);
	break;
    default:
	devx = 0;	/* for -Wall */
	devy = 0;
	BadUnitsError("GConvert");
    }

    switch (to) {
    case DEVICE:
	*x = devx;
	*y = devy;
	break;
    case NDC:
	*x = xDevtoNDC(devx, dd);
	*y = yDevtoNDC(devy, dd);
	break;
    case INCHES:
	*x = xDevtoInch(devx, dd);
	*y = yDevtoInch(devy, dd);
	break;
    case LINES:
	*x = xDevtoLine(devx, dd);
	*y = yDevtoLine(devy, dd);
	break;
    case NIC:
	*x = xDevtoNIC(devx, dd);
	*y = yDevtoNIC(devy, dd);
	break;
    case OMA1:
	*x = xDevtoOMA1(devx, dd);
	*y = yDevtoOMA1(devy, dd);
	break;
    case OMA2:
	*x = yDevtoxOMA2(devy, dd);
	*y = xDevtoyOMA2(devx, dd);
	break;
    case OMA3:
	*x = xDevtoOMA3(devx, dd);
	*y = yDevtoOMA3(devy, dd);
	break;
    case OMA4:
	*x = yDevtoxOMA4(devy, dd);
	*y = xDevtoyOMA4(devx, dd);
	break;
    case NFC:
	*x = xDevtoNFC(devx, dd);
	*y = yDevtoNFC(devy, dd);
	break;
    case NPC:
	*x = xDevtoNPC(devx, dd);
	*y = yDevtoNPC(devy, dd);
	break;
    case USER:
	*x = xDevtoUsr(devx, dd);
	*y = yDevtoUsr(devy, dd);
	break;
    case MAR1:
	*x = xDevtoMAR1(devx, dd);
	*y = yDevtoMAR1(devy, dd);
	break;
    case MAR2:
	*x = yDevtoxMAR2(devy, dd);
	*y = xDevtoyMAR2(devx, dd);
	break;
    case MAR3:
	*x = xDevtoMAR3(devx, dd);
	*y = yDevtoMAR3(devy, dd);
	break;
    case MAR4:
	*x = yDevtoxMAR4(devy, dd);
	*y = xDevtoyMAR4(devx, dd);
	break;
    default:
	BadUnitsError("GConvert");
    }
}

double GConvertX(double x, GUnit from, GUnit to, DevDesc *dd)
{
    double devx;
    switch (from) {
    case DEVICE:devx = x;	break;
    case NDC:	devx = xNDCtoDev(x, dd);	break;
    case INCHES:devx = xInchtoDev(x, dd);	break;
    case LINES: devx = xLinetoDev(x, dd);       break;
    case OMA1:	devx = xOMA1toDev(x, dd);	break;
    /*case OMA2:	x <--> y */
    case OMA3:	devx = xOMA3toDev(x, dd);	break;
    /*case OMA4:	x <--> y */
    case NIC:	devx = xNICtoDev(x, dd);	break;
    case NFC:	devx = xNFCtoDev(x, dd);	break;
    case MAR1:	devx = xMAR1toDev(x, dd);	break;
    /*case MAR2:	x <--> y */
    case MAR3:	devx = xMAR3toDev(x, dd);	break;
    /*case MAR4:	x <--> y */
    case NPC:	devx = xNPCtoDev(x, dd);	break;
    case USER:	devx = xUsrtoDev(x, dd);	break;
    default:	devx = 0;/* for -Wall */ BadUnitsError("GConvertX");
    }

    switch (to) {
    case DEVICE:x = devx;	break;
    case NDC:	x = xDevtoNDC(devx, dd);	break;
    case INCHES:x = xDevtoInch(devx, dd);	break;
    case LINES:	x = xDevtoLine(devx, dd);	break;
    case NIC:	x = xDevtoNIC(devx, dd);	break;
    case OMA1:	x = xDevtoOMA1(devx, dd);	break;
    /*case OMA2:	x <--> y */
    case OMA3:	x = xDevtoOMA3(devx, dd);	break;
    /*case OMA4:	x <--> y */
    case NFC:	x = xDevtoNFC(devx, dd);	break;
    case USER:	x = xDevtoUsr(devx, dd);	break;
    case MAR1:	x = xDevtoMAR1(devx, dd);	break;
    /*case MAR2:	x <--> y */
    case MAR3:	x = xDevtoMAR3(devx, dd);	break;
    /*case MAR4:	x <--> y */
    default:	BadUnitsError("GConvertX");
    }
    return x;
}

double GConvertY(double y, GUnit from, GUnit to, DevDesc *dd)
{
    double devy;
    switch (from) {
    case DEVICE:devy = y;	break;
    case NDC:	devy = yNDCtoDev(y, dd);	break;
    case INCHES:devy = yInchtoDev(y, dd);	break;
    case LINES: devy = yLinetoDev(y, dd);       break;
    case OMA1:	devy = yOMA1toDev(y, dd);	break;
    /*case OMA2:	x <--> y */
    case OMA3:	devy = yOMA3toDev(y, dd);	break;
    /*case OMA4:	x <--> y */
    case NIC:	devy = yNICtoDev(y, dd);	break;
    case NFC:	devy = yNFCtoDev(y, dd);	break;
    case MAR1:	devy = yMAR1toDev(y, dd);	break;
    /*case MAR2:	x <--> y */
    case MAR3:	devy = yMAR3toDev(y, dd);	break;
    /*case MAR4:	x <--> y */
    case NPC:	devy = yNPCtoDev(y, dd);	break;
    case USER:	devy = yUsrtoDev(y, dd);	break;
    default:	devy = 0;/* for -Wall */ BadUnitsError("GConvertY");
    }

    switch (to) {
    case DEVICE:y = devy;	break;
    case NDC:	y = yDevtoNDC(devy, dd);	break;
    case INCHES:y = yDevtoInch(devy, dd);	break;
    case LINES:	y = yDevtoLine(devy, dd);	break;
    case NIC:	y = yDevtoNIC(devy, dd);	break;
    case OMA1:	y = yDevtoOMA1(devy, dd);	break;
    /*case OMA2:	x <--> y */
    case OMA3:	y = yDevtoOMA3(devy, dd);	break;
    /*case OMA4:	x <--> y */
    case NFC:	y = yDevtoNFC(devy, dd);	break;
    case USER:	y = yDevtoUsr(devy, dd);	break;
    case MAR1:	y = yDevtoMAR1(devy, dd);	break;
    /*case MAR2:	x <--> y */
    case MAR3:	y = yDevtoMAR3(devy, dd);	break;
    /*case MAR4:	x <--> y */
    default:	BadUnitsError("GConvertY");
    }
    return y;
}

/* Code for layouts */

static double sum(double values[], int n, int cmValues[], int cmSum)
{
    int i;
    double s = 0;
    for (i = 0; i < n; i++)
	if ((cmSum && cmValues[i]) || (!cmSum && !cmValues[i]))
	    s = s + values[i];
    return s;
}

static double sumWidths(DevDesc *dd)
{
    return sum(Rf_gpptr(dd)->widths, Rf_gpptr(dd)->numcols, Rf_gpptr(dd)->cmWidths, 0);
}

static double sumCmWidths(DevDesc *dd)
{
    return sum(Rf_gpptr(dd)->widths, Rf_gpptr(dd)->numcols,  Rf_gpptr(dd)->cmWidths, 1);
}

static double sumHeights(DevDesc *dd)
{
    return sum(Rf_gpptr(dd)->heights, Rf_gpptr(dd)->numrows, Rf_gpptr(dd)->cmHeights, 0);
}

static double sumCmHeights(DevDesc *dd)
{
    return sum(Rf_gpptr(dd)->heights, Rf_gpptr(dd)->numrows, Rf_gpptr(dd)->cmHeights, 1);
}

static int tallLayout(double cmWidth, double cmHeight, DevDesc *dd)
{
    return (cmHeight/sumHeights(dd)) > (cmWidth/sumWidths(dd));
}

static void figureExtent(int *minCol, int *maxCol, int *minRow, int *maxRow,
			 int figureNum, DevDesc *dd)
{
    int minc = -1;
    int maxc = -1;
    int minr = -1;
    int maxr = -1;
    int i, j;
    for (i = 0; i < Rf_gpptr(dd)->numrows; i++)
	for (j = 0; j < Rf_gpptr(dd)->numcols; j++)
	    if (Rf_gpptr(dd)->order[i][j] == figureNum) {
		if ((minc == -1) || (j < minc))
		    minc = j;
		if ((maxc == -1) || (j > maxc))
		    maxc = j;
		if ((minr == -1) || (i < minr))
		    minr = i;
		if ((maxr == -1) || (i > maxr))
		    maxr = i;
	    }
    *minCol = minc;
    *maxCol = maxc;
    *minRow = minr;
    *maxRow = maxr;
}

static double sumRegions(double regions[], int from, int to)
{
    int i;
    double s = 0;
    for (i = from; i < to + 1; i++)
	s = s + regions[i];
    return s;
}

static void largestRegion(double *width, double *height,
			  double layoutAspectRatio, double innerAspectRatio)
{
    if (layoutAspectRatio < innerAspectRatio) {
	*width = 1.0;
	*height = layoutAspectRatio/innerAspectRatio;
    }
    else {
	*width = innerAspectRatio/layoutAspectRatio;
	*height = 1.0;
    }
}

static void layoutRegion(double *width, double *height,
			 double widths[], double heights[],
			 double cmWidth, double cmHeight, DevDesc *dd)
{
  largestRegion(width, height,
		sum(heights, Rf_gpptr(dd)->numrows, Rf_gpptr(dd)->cmHeights, 0)/
		sum(widths, Rf_gpptr(dd)->numcols, Rf_gpptr(dd)->cmWidths, 0),
		cmHeight/cmWidth);
}


	/* allocate one dimension (width or height) for either */
	/* relative or cm units */

static void allocDimension(double dimensions[], double sumDimensions, int n,
			   int cmDimensions[], int cmDimension)
{
    int i;
    for (i = 0; i < n; i++)
	if ((cmDimension && cmDimensions[i]) ||
	    (!cmDimension && !cmDimensions[i]))
	    dimensions[i] = dimensions[i]/sumDimensions;
}

static void allCmRegions(double widths[], double heights[],
			 double cmWidth, double cmHeight, DevDesc *dd)
{
    allocDimension(widths, cmWidth, Rf_gpptr(dd)->numcols, Rf_gpptr(dd)->cmWidths, 1);
    allocDimension(heights, cmHeight, Rf_gpptr(dd)->numrows, Rf_gpptr(dd)->cmHeights, 1);
}

static void modifyDimension(double dimension[], double multiplier, double n,
			    int cmDimensions[])
{
    int i;
    for (i = 0; i < n; i++)
	if (!cmDimensions[i])
	    dimension[i] = dimension[i] * multiplier;
}

static void modifyRegions(double widths[], double heights[],
		   double colMultiplier, double rowMultiplier,
		   DevDesc *dd)
{
    modifyDimension(widths, colMultiplier, Rf_gpptr(dd)->numcols, Rf_gpptr(dd)->cmWidths);
    modifyDimension(heights, rowMultiplier, Rf_gpptr(dd)->numrows, Rf_gpptr(dd)->cmHeights);
}

static void regionsWithoutRespect(double widths[], double heights[], DevDesc *dd)
{
    allocDimension(widths,
		   sum(widths, Rf_gpptr(dd)->numcols, Rf_gpptr(dd)->cmWidths, 0),
		   Rf_gpptr(dd)->numcols, Rf_gpptr(dd)->cmWidths, 0);
    allocDimension(heights,
		   sum(heights, Rf_gpptr(dd)->numrows, Rf_gpptr(dd)->cmHeights, 0),
		   Rf_gpptr(dd)->numrows, Rf_gpptr(dd)->cmHeights, 0);
}

static void regionsWithRespect(double widths[], double heights[],
			       double cmWidth, double cmHeight, DevDesc *dd)
{
    double cm, rm;
    layoutRegion(&cm, &rm, widths, heights, cmWidth, cmHeight, dd);
    regionsWithoutRespect(widths, heights, dd);
    modifyRegions(widths, heights, cm, rm, dd);
}

static void widthsRespectingHeights(double widths[],
				    double cmWidth, double cmHeight,
				    DevDesc *dd)
{
    int i, j;
    int respectedCols[MAX_LAYOUT_COLS];
    double widthLeft;
    double disrespectedWidth = 0;
    for (j = 0; j < Rf_gpptr(dd)->numcols; j++) {
	respectedCols[j] = 0;
	widths[j] = Rf_gpptr(dd)->widths[j];
    }
    for (i = 0; i < Rf_gpptr(dd)->numrows; i++)
	for (j = 0; j < Rf_gpptr(dd)->numcols; j++)
	    if (Rf_gpptr(dd)->respect[i][j] && !Rf_gpptr(dd)->cmWidths[j])
		respectedCols[j] = 1;
    for (j = 0; j < Rf_gpptr(dd)->numcols; j++)
	if (!respectedCols[j])
	    disrespectedWidth += Rf_gpptr(dd)->widths[j];
    widthLeft = sumHeights(dd) * cmWidth/cmHeight -
	sumWidths(dd) + disrespectedWidth;
    for (j = 0; j < Rf_gpptr(dd)->numcols; j++)
	if (!respectedCols[j])
	    widths[j] = widthLeft * widths[j]/disrespectedWidth;
}

static void regionsRespectingHeight(double widths[], double heights[],
			     double cmWidth, double cmHeight,
			     DevDesc *dd)
{
    widthsRespectingHeights(widths, cmWidth, cmHeight, dd);
    regionsWithRespect(widths, heights, cmWidth, cmHeight, dd);
}

static void heightsRespectingWidths(double heights[],
			     double cmWidth, double cmHeight,
			     DevDesc *dd)
{
    int i, j;
    int respectedRows[MAX_LAYOUT_ROWS];
    double heightLeft;
    double disrespectedHeight = 0;
    for (i = 0; i < Rf_gpptr(dd)->numrows; i++) {
	respectedRows[i] = 0;
	heights[i] = Rf_gpptr(dd)->heights[i];
    }
    for (i = 0; i < Rf_gpptr(dd)->numrows; i++)
	for (j = 0; j < Rf_gpptr(dd)->numcols; j++)
	    if (Rf_gpptr(dd)->respect[i][j] && !Rf_gpptr(dd)->cmHeights[i])
		respectedRows[i] = 1;
    for (i = 0; i < Rf_gpptr(dd)->numrows; i++)
	if (!respectedRows[i])
	    disrespectedHeight += Rf_gpptr(dd)->heights[i];
    heightLeft = sumWidths(dd) * cmHeight/cmWidth -
	sumHeights(dd) + disrespectedHeight;
    for (i = 0; i < Rf_gpptr(dd)->numrows; i++)
	if (!respectedRows[i])
	    heights[i] = heightLeft * heights[i]/disrespectedHeight;
}

static void regionsRespectingWidth(double widths[], double heights[],
			    double cmWidth, double cmHeight,
			    DevDesc *dd)
{
    heightsRespectingWidths(heights, cmWidth, cmHeight, dd);
    regionsWithRespect(widths, heights, cmWidth, cmHeight, dd);
}

static void noCmRegions(double widths[], double heights[],
		 double cmWidth, double cmHeight, DevDesc *dd)
{
    switch (Rf_gpptr(dd)->rspct) {
    case 0:
	regionsWithoutRespect(widths, heights, dd);
	break;
    case 1:
	regionsWithRespect(widths, heights, cmWidth, cmHeight, dd);
	break;
    case 2:
	if (tallLayout(cmWidth, cmHeight, dd))
	    regionsRespectingWidth(widths, heights, cmWidth, cmHeight, dd);
	else
	    regionsRespectingHeight(widths, heights, cmWidth, cmHeight, dd);
    }
}

static void notAllCmRegions(double widths[], double heights[],
			    double cmWidth, double cmHeight, DevDesc *dd)
{
    double newCmWidth, newCmHeight;
    newCmWidth = cmWidth - sumCmWidths(dd);
    newCmHeight = cmHeight - sumCmHeights(dd);
    noCmRegions(widths, heights, newCmWidth, newCmHeight, dd);
    allocDimension(widths, cmWidth, Rf_gpptr(dd)->numcols, Rf_gpptr(dd)->cmWidths, 1);
    allocDimension(heights, cmHeight, Rf_gpptr(dd)->numrows, Rf_gpptr(dd)->cmHeights, 1);
    modifyDimension(widths, newCmWidth/cmWidth, Rf_gpptr(dd)->numcols,
		    Rf_gpptr(dd)->cmWidths);
    modifyDimension(heights, newCmHeight/cmHeight, Rf_gpptr(dd)->numrows,
		    Rf_gpptr(dd)->cmHeights);
}

static void widthCmRegions(double widths[], double heights[],
			   double cmWidth, double cmHeight, DevDesc *dd)
{
    allocDimension(widths, cmWidth, Rf_gpptr(dd)->numcols, Rf_gpptr(dd)->cmWidths, 1);
    allocDimension(heights, sumHeights(dd), Rf_gpptr(dd)->numrows,
		   Rf_gpptr(dd)->cmHeights, 0);
    modifyDimension(heights, (cmHeight - sumCmHeights(dd))/cmHeight,
		    Rf_gpptr(dd)->numrows, Rf_gpptr(dd)->cmHeights);
    allocDimension(heights, cmHeight, Rf_gpptr(dd)->numrows,
		   Rf_gpptr(dd)->cmHeights, 1);
}

static void heightCmRegions(double widths[], double heights[],
			    double cmWidth, double cmHeight, DevDesc *dd)
{
    allocDimension(heights, cmHeight, Rf_gpptr(dd)->numrows, Rf_gpptr(dd)->cmHeights, 1);
    allocDimension(widths, sumWidths(dd), Rf_gpptr(dd)->numcols,
		   Rf_gpptr(dd)->cmWidths, 0);
    modifyDimension(widths, (cmWidth - sumCmWidths(dd))/cmWidth,
		    Rf_gpptr(dd)->numcols, Rf_gpptr(dd)->cmWidths);
    allocDimension(widths, cmWidth, Rf_gpptr(dd)->numcols,
		   Rf_gpptr(dd)->cmWidths, 1);
}

static Rboolean allCmWidths(DevDesc *dd)
{
    int j;
    for (j = 0; j < Rf_gpptr(dd)->numcols; j++)
	if (!Rf_gpptr(dd)->cmWidths[j])
	    return FALSE;
    return TRUE;
}

static Rboolean allCmHeights(DevDesc *dd)
{
    int i;
    for (i = 0; i < Rf_gpptr(dd)->numrows; i++)
	if (!Rf_gpptr(dd)->cmHeights[i])
	    return FALSE;
    return TRUE;
}

static Rboolean noCmWidths(DevDesc *dd)
{
    int j;
    for (j = 0; j < Rf_gpptr(dd)->numcols; j++)
	if (Rf_gpptr(dd)->cmWidths[j])
	    return FALSE;
    return TRUE;
}

static Rboolean noCmHeights(DevDesc *dd)
{
    int i;
    for (i = 0; i < Rf_gpptr(dd)->numrows; i++)
	if (Rf_gpptr(dd)->cmHeights[i])
	    return FALSE;
    return TRUE;
}

static void someCmRegions(double widths[], double heights[],
		   double cmWidth, double cmHeight, DevDesc *dd)
{
    if (allCmWidths(dd))
	widthCmRegions(widths, heights, cmWidth, cmHeight, dd);
    else if (allCmHeights(dd))
	heightCmRegions(widths, heights, cmWidth, cmHeight, dd);
    else
	notAllCmRegions(widths, heights, cmWidth, cmHeight, dd);
}

static Rboolean allCm(DevDesc *dd)
{
    return allCmWidths(dd) && allCmHeights(dd);
}

static Rboolean noCm(DevDesc *dd)
{
    return noCmWidths(dd) && noCmHeights(dd);
}

static void layoutRegions(double widths[], double heights[],
		   double cmWidth, double cmHeight, DevDesc *dd)
{
    int i, j;
    for (j = 0; j < Rf_gpptr(dd)->numcols; j++)
	widths[j] = Rf_gpptr(dd)->widths[j];
    for (i = 0; i < Rf_gpptr(dd)->numrows; i++)
	heights[i] = Rf_gpptr(dd)->heights[i];

    if (allCm(dd))
	allCmRegions(widths, heights, cmWidth, cmHeight, dd);
    else if (noCm(dd))
	noCmRegions(widths, heights, cmWidth, cmHeight, dd);
    else
	someCmRegions(widths, heights, cmWidth, cmHeight, dd);
}

static void subRegion(double *left, double *right, double *bottom, double *top,
		      double mincol, double maxcol,
		      double minrow, double maxrow,
		      double widths[], double heights[], DevDesc *dd)
{
    double totalWidth = sumRegions(widths, 0, Rf_gpptr(dd)->numcols-1);
    double totalHeight = sumRegions(heights, 0, Rf_gpptr(dd)->numrows-1);
    *left = (0.5 - totalWidth/2) + sumRegions(widths, 0, mincol-1);
    *right = (0.5 - totalWidth/2) + sumRegions(widths, 0, maxcol);
    *bottom = (0.5 - totalHeight/2) + totalHeight
	- sumRegions(heights, 0, maxrow);
    *top = (0.5 - totalHeight/2) + totalHeight
	- sumRegions(heights, 0, minrow-1);
}

/* a fudge for backwards compatibility (of sorts) with par(mfg) */
/* return the top-left-most row/col that the current figure */
/* occupies in the current layout */

void currentFigureLocation(int *row, int *col, DevDesc *dd)
{
    int maxcol, maxrow;
    if (Rf_gpptr(dd)->layout)
	figureExtent(col, &maxcol, row, &maxrow, Rf_gpptr(dd)->currentFigure, dd);
    else if (Rf_gpptr(dd)->mfind) { /* mfcol */
	*row = (Rf_gpptr(dd)->currentFigure - 1)%Rf_gpptr(dd)->numrows;
	*col = (Rf_gpptr(dd)->currentFigure - 1)/Rf_gpptr(dd)->numrows;
    }
    else { /* mfrow */
	*row = (Rf_gpptr(dd)->currentFigure - 1)/Rf_gpptr(dd)->numcols;
	*col = (Rf_gpptr(dd)->currentFigure - 1)%Rf_gpptr(dd)->numcols;
    }
}

/*  mapNDC2Dev -- transformation from NDC to Dev  */
/*  Use this coordinate system for outer margin coordinates  */
/*  This must be called if the device is resized */

static void mapNDC2Dev(DevDesc *dd)
{
    /* For new devices, have to check the device's idea of its size
     * in case there has been a resize.
     */
    if (dd->newDevStruct) {
	Rf_gpptr(dd)->ndc2dev.bx = Rf_dpptr(dd)->ndc2dev.bx =
	    (((GEDevDesc*) dd)->dev->right - ((GEDevDesc*) dd)->dev->left);
	Rf_gpptr(dd)->ndc2dev.ax = Rf_dpptr(dd)->ndc2dev.ax =
	    ((GEDevDesc*) dd)->dev->left;
	Rf_gpptr(dd)->ndc2dev.by = Rf_dpptr(dd)->ndc2dev.by =
	    (((GEDevDesc*) dd)->dev->top - ((GEDevDesc*) dd)->dev->bottom);
	Rf_gpptr(dd)->ndc2dev.ay = Rf_dpptr(dd)->ndc2dev.ay =
	    ((GEDevDesc*) dd)->dev->bottom;
    } else {
	Rf_gpptr(dd)->ndc2dev.bx = Rf_dpptr(dd)->ndc2dev.bx = (Rf_gpptr(dd)->right - Rf_gpptr(dd)->left);
	Rf_gpptr(dd)->ndc2dev.ax = Rf_dpptr(dd)->ndc2dev.ax = Rf_gpptr(dd)->left;
	Rf_gpptr(dd)->ndc2dev.by = Rf_dpptr(dd)->ndc2dev.by = (Rf_gpptr(dd)->top - Rf_gpptr(dd)->bottom);
	Rf_gpptr(dd)->ndc2dev.ay = Rf_dpptr(dd)->ndc2dev.ay = Rf_gpptr(dd)->bottom;
    }
    /* Units Conversion */

    Rf_gpptr(dd)->xNDCPerInch = Rf_dpptr(dd)->xNDCPerInch =
	1.0/fabs(Rf_gpptr(dd)->ndc2dev.bx * Rf_gpptr(dd)->ipr[0]);
    Rf_gpptr(dd)->yNDCPerInch = Rf_dpptr(dd)->yNDCPerInch =
	1.0/fabs(Rf_gpptr(dd)->ndc2dev.by * Rf_gpptr(dd)->ipr[1]);
    Rf_gpptr(dd)->xNDCPerChar = Rf_dpptr(dd)->xNDCPerChar =
	fabs(Rf_gpptr(dd)->cexbase *
	     Rf_gpptr(dd)->cra[1] * Rf_gpptr(dd)->asp / Rf_gpptr(dd)->ndc2dev.bx);
    Rf_gpptr(dd)->yNDCPerChar = Rf_dpptr(dd)->yNDCPerChar =
	fabs(Rf_gpptr(dd)->cexbase *
	     Rf_gpptr(dd)->cra[1] / Rf_gpptr(dd)->ndc2dev.by);
    Rf_gpptr(dd)->xNDCPerLine = Rf_dpptr(dd)->xNDCPerLine =
	fabs(Rf_gpptr(dd)->mex * Rf_gpptr(dd)->cexbase *
	     Rf_gpptr(dd)->cra[1] * Rf_gpptr(dd)->asp / Rf_gpptr(dd)->ndc2dev.bx);
    Rf_gpptr(dd)->yNDCPerLine = Rf_dpptr(dd)->yNDCPerLine =
	fabs(Rf_gpptr(dd)->mex * Rf_gpptr(dd)->cexbase *
	     Rf_gpptr(dd)->cra[1] / Rf_gpptr(dd)->ndc2dev.by);
}

static void updateOuterMargins(DevDesc *dd)
{
    switch (Rf_gpptr(dd)->oUnits) {
    case LINES:
	Rf_gpptr(dd)->omi[0] = Rf_dpptr(dd)->omi[0] =
	    GConvertYUnits(Rf_gpptr(dd)->oma[0], LINES, INCHES, dd);
	Rf_gpptr(dd)->omi[1] = Rf_dpptr(dd)->omi[1] =
	    GConvertXUnits(Rf_gpptr(dd)->oma[1], LINES, INCHES, dd);
	Rf_gpptr(dd)->omi[2] = Rf_dpptr(dd)->omi[2] =
	    GConvertYUnits(Rf_gpptr(dd)->oma[2], LINES, INCHES, dd);
	Rf_gpptr(dd)->omi[3] = Rf_dpptr(dd)->omi[3] =
	    GConvertXUnits(Rf_gpptr(dd)->oma[3], LINES, INCHES, dd);
	Rf_gpptr(dd)->omd[0] = Rf_dpptr(dd)->omd[0] =
	    GConvertYUnits(Rf_gpptr(dd)->oma[0], LINES, NDC, dd);
	Rf_gpptr(dd)->omd[1] = Rf_dpptr(dd)->omd[1] =
	    GConvertXUnits(Rf_gpptr(dd)->oma[1], LINES, NDC, dd);
	Rf_gpptr(dd)->omd[2] = Rf_dpptr(dd)->omd[2] =
	    GConvertYUnits(Rf_gpptr(dd)->oma[2], LINES, NDC, dd);
	Rf_gpptr(dd)->omd[3] = Rf_dpptr(dd)->omd[3] =
	    GConvertXUnits(Rf_gpptr(dd)->oma[3], LINES, NDC, dd);
	break;
    case INCHES:
	Rf_gpptr(dd)->oma[0] = Rf_dpptr(dd)->oma[0] =
	    GConvertYUnits(Rf_gpptr(dd)->omi[0], INCHES, LINES, dd);
	Rf_gpptr(dd)->oma[1] = Rf_dpptr(dd)->oma[1] =
	    GConvertXUnits(Rf_gpptr(dd)->omi[1], INCHES, LINES, dd);
	Rf_gpptr(dd)->oma[2] = Rf_dpptr(dd)->oma[2] =
	    GConvertYUnits(Rf_gpptr(dd)->omi[2], INCHES, LINES, dd);
	Rf_gpptr(dd)->oma[3] = Rf_dpptr(dd)->oma[3] =
	    GConvertXUnits(Rf_gpptr(dd)->omi[3], INCHES, LINES, dd);
	Rf_gpptr(dd)->omd[0] = Rf_dpptr(dd)->omd[0] =
	    GConvertYUnits(Rf_gpptr(dd)->omi[0], INCHES, NDC, dd);
	Rf_gpptr(dd)->omd[1] = Rf_dpptr(dd)->omd[1] =
	    GConvertXUnits(Rf_gpptr(dd)->omi[1], INCHES, NDC, dd);
	Rf_gpptr(dd)->omd[2] = Rf_dpptr(dd)->omd[2] =
	    GConvertYUnits(Rf_gpptr(dd)->omi[2], INCHES, NDC, dd);
	Rf_gpptr(dd)->omd[3] = Rf_dpptr(dd)->omd[3] =
	    GConvertXUnits(Rf_gpptr(dd)->omi[3], INCHES, NDC, dd);
	break;
    case NDC:
	Rf_gpptr(dd)->oma[0] = Rf_dpptr(dd)->oma[0] =
	    GConvertYUnits(Rf_gpptr(dd)->omd[0], NDC, LINES, dd);
	Rf_gpptr(dd)->oma[1] = Rf_dpptr(dd)->oma[1] =
	    GConvertXUnits(Rf_gpptr(dd)->omd[1], NDC, LINES, dd);
	Rf_gpptr(dd)->oma[2] = Rf_dpptr(dd)->oma[2] =
	    GConvertYUnits(Rf_gpptr(dd)->omd[2], NDC, LINES, dd);
	Rf_gpptr(dd)->oma[3] = Rf_dpptr(dd)->oma[3] =
	    GConvertXUnits(Rf_gpptr(dd)->omd[3], NDC, LINES, dd);
	Rf_gpptr(dd)->omi[0] = Rf_dpptr(dd)->omi[0] =
	    GConvertYUnits(Rf_gpptr(dd)->omd[0], NDC, INCHES, dd);
	Rf_gpptr(dd)->omi[1] = Rf_dpptr(dd)->omi[1] =
	    GConvertXUnits(Rf_gpptr(dd)->omd[1], NDC, INCHES, dd);
	Rf_gpptr(dd)->omi[2] = Rf_dpptr(dd)->omi[2] =
	    GConvertYUnits(Rf_gpptr(dd)->omd[2], NDC, INCHES, dd);
	Rf_gpptr(dd)->omi[3] = Rf_dpptr(dd)->omi[3] =
	    GConvertXUnits(Rf_gpptr(dd)->omd[3], NDC, INCHES, dd);
	break;
    default: break; /*nothing (-Wall) */
    }
}

/*  mapInner2Dev -- transformation from NIC to Dev  */
/*  Use this coordinate system for setting up multiple figures	*/
/*  This is also used when specifying the figure region directly  */
/*  Note that this is incompatible with S which uses then entire  */
/*  device surface for such a plot  */
/*  This must be called per DevNewPlot, if the NDCtoDev transformation */
/*  changes, and if oma changes */

static void mapInner2Dev(DevDesc *dd)
{
    double x0, x1, y0, y1;
    x0 = xLinetoDev(Rf_gpptr(dd)->oma[1], dd);
    y0 = yLinetoDev(Rf_gpptr(dd)->oma[0], dd);
    x1 = GConvertXUnits(Rf_gpptr(dd)->oma[3], LINES, NDC, dd);
    x1 = xNDCtoDev(1.0 - x1, dd);
    y1 = GConvertYUnits(Rf_gpptr(dd)->oma[2], LINES, NDC, dd);
    y1 = yNDCtoDev(1.0 - y1, dd);
    Rf_gpptr(dd)->inner2dev.bx = Rf_dpptr(dd)->inner2dev.bx = x1 - x0;
    Rf_gpptr(dd)->inner2dev.ax = Rf_dpptr(dd)->inner2dev.ax = x0;
    Rf_gpptr(dd)->inner2dev.by = Rf_dpptr(dd)->inner2dev.by = y1 - y0;
    Rf_gpptr(dd)->inner2dev.ay = Rf_dpptr(dd)->inner2dev.ay = y0;
}

/* mapFigureRegion -- calculate figure region in NIC  */

static void mapFigureRegion(DevDesc *dd)
{
    int mincol, maxcol, minrow, maxrow;
    double x0, x1, y0, y1;
    double widths[MAX_LAYOUT_COLS], heights[MAX_LAYOUT_ROWS];
    if (Rf_gpptr(dd)->layout) {
	layoutRegions(widths, heights,
		      GConvertXUnits(1.0, NIC, INCHES, dd)*2.54,
		      GConvertYUnits(1.0, NIC, INCHES, dd)*2.54, dd);
	figureExtent(&mincol, &maxcol, &minrow, &maxrow,
		     Rf_gpptr(dd)->currentFigure, dd);
	subRegion(&x0, &x1, &y0, &y1,
		  mincol, maxcol, minrow, maxrow,
		  widths, heights, dd);
    }
    else {
	int row, col;
	if (Rf_gpptr(dd)->mfind) {
	    col = (Rf_gpptr(dd)->currentFigure-1) / Rf_gpptr(dd)->numrows + 1;
	    row = Rf_gpptr(dd)->currentFigure - (col-1)*Rf_gpptr(dd)->numrows;
	}
	else {
	    row = (Rf_gpptr(dd)->currentFigure-1) / Rf_gpptr(dd)->numcols + 1;
	    col = Rf_gpptr(dd)->currentFigure - (row-1)*Rf_gpptr(dd)->numcols;
	}
	x0 = (double) (col-1) / Rf_gpptr(dd)->numcols;
	x1 = (double) col / Rf_gpptr(dd)->numcols;
	y0 = (double) (Rf_gpptr(dd)->numrows - row) / Rf_gpptr(dd)->numrows;
	y1 = (double) (Rf_gpptr(dd)->numrows - row + 1) / Rf_gpptr(dd)->numrows;
    }
    Rf_gpptr(dd)->fig[0] = Rf_dpptr(dd)->fig[0] = x0;
    Rf_gpptr(dd)->fig[1] = Rf_dpptr(dd)->fig[1] = x1;
    Rf_gpptr(dd)->fig[2] = Rf_dpptr(dd)->fig[2] = y0;
    Rf_gpptr(dd)->fig[3] = Rf_dpptr(dd)->fig[3] = y1;
    Rf_gpptr(dd)->fUnits = Rf_dpptr(dd)->fUnits = NIC;
}

static void updateFigureRegion(DevDesc *dd)
{
    double nicWidth, nicHeight;
    switch (Rf_gpptr(dd)->fUnits) {
    case NIC:
	Rf_gpptr(dd)->fin[0] = Rf_dpptr(dd)->fin[0] =
	    GConvertXUnits(Rf_gpptr(dd)->fig[1] - Rf_gpptr(dd)->fig[0], NIC, INCHES, dd);
	Rf_gpptr(dd)->fin[1] = Rf_dpptr(dd)->fin[1] =
	    GConvertYUnits(Rf_gpptr(dd)->fig[3] - Rf_gpptr(dd)->fig[2], NIC, INCHES, dd);
	break;
    case INCHES:
	nicWidth = GConvertXUnits(Rf_gpptr(dd)->fin[0], INCHES, NIC, dd);
	nicHeight = GConvertYUnits(Rf_gpptr(dd)->fin[1], INCHES, NIC, dd);
	Rf_gpptr(dd)->fig[0] = Rf_dpptr(dd)->fig[0] = 0.5 - nicWidth/2;
	Rf_gpptr(dd)->fig[1] = Rf_dpptr(dd)->fig[1] = Rf_gpptr(dd)->fig[0] + nicWidth;
	Rf_gpptr(dd)->fig[2] = Rf_dpptr(dd)->fig[2] = 0.5 - nicHeight/2;
	Rf_gpptr(dd)->fig[3] = Rf_dpptr(dd)->fig[3] = Rf_gpptr(dd)->fig[2] + nicHeight;
	break;
    default: /*nothing*/ break;
    }
}

/*  mapFig2Dev -- Transformation from NFC to Dev  */
/* This must be called per plot.new and if the NICtoDev transformation */
/* changes */

static void mapFig2Dev(DevDesc *dd)
{
    double x0, x1, y0, y1;
    y0 = yNICtoDev(Rf_gpptr(dd)->fig[2], dd);
    y1 = yNICtoDev(Rf_gpptr(dd)->fig[3], dd);
    x0 = xNICtoDev(Rf_gpptr(dd)->fig[0], dd);
    x1 = xNICtoDev(Rf_gpptr(dd)->fig[1], dd);
    Rf_gpptr(dd)->fig2dev.bx = Rf_dpptr(dd)->fig2dev.bx = x1 - x0;
    Rf_gpptr(dd)->fig2dev.ax = Rf_dpptr(dd)->fig2dev.ax = x0;
    Rf_gpptr(dd)->fig2dev.by = Rf_dpptr(dd)->fig2dev.by = y1 - y0;
    Rf_gpptr(dd)->fig2dev.ay = Rf_dpptr(dd)->fig2dev.ay = y0;
}

static void updateFigureMargins(DevDesc *dd)
{
    switch (Rf_gpptr(dd)->mUnits) {
    case LINES:
	Rf_gpptr(dd)->mai[0] = Rf_dpptr(dd)->mai[0] =
	    GConvertYUnits(Rf_gpptr(dd)->mar[0], LINES, INCHES, dd);
	Rf_gpptr(dd)->mai[1] = Rf_dpptr(dd)->mai[1] =
	    GConvertXUnits(Rf_gpptr(dd)->mar[1], LINES, INCHES, dd);
	Rf_gpptr(dd)->mai[2] = Rf_dpptr(dd)->mai[2] =
	    GConvertYUnits(Rf_gpptr(dd)->mar[2], LINES, INCHES, dd);
	Rf_gpptr(dd)->mai[3] = Rf_dpptr(dd)->mai[3] =
	    GConvertXUnits(Rf_gpptr(dd)->mar[3], LINES, INCHES, dd);
	break;
    case INCHES:
	Rf_gpptr(dd)->mar[0] = Rf_dpptr(dd)->mar[0] =
	    GConvertYUnits(Rf_gpptr(dd)->mai[0], INCHES, LINES, dd);
	Rf_gpptr(dd)->mar[1] = Rf_dpptr(dd)->mar[1] =
	    GConvertXUnits(Rf_gpptr(dd)->mai[1], INCHES, LINES, dd);
	Rf_gpptr(dd)->mar[2] = Rf_dpptr(dd)->mar[2] =
	    GConvertYUnits(Rf_gpptr(dd)->mai[2], INCHES, LINES, dd);
	Rf_gpptr(dd)->mar[3] = Rf_dpptr(dd)->mar[3] =
	    GConvertXUnits(Rf_gpptr(dd)->mai[3], INCHES, LINES, dd);
	break;
    default: /*nothing*/ break;
    }
}

/* mapPlotRegion -- plot region in NFC */

static void mapPlotRegion(DevDesc *dd)
{
    double x0, x1, y0, y1;
    x0 = GConvertXUnits(Rf_gpptr(dd)->mar[1], LINES, NFC, dd);
    y0 = GConvertYUnits(Rf_gpptr(dd)->mar[0], LINES, NFC, dd);
    x1 = 1.0 - GConvertXUnits(Rf_gpptr(dd)->mar[3], LINES, NFC, dd);
    y1 = 1.0 - GConvertYUnits(Rf_gpptr(dd)->mar[2], LINES, NFC, dd);
    if(Rf_gpptr(dd)->pty == 's') {
	/* maximal plot size in inches */
	double center, width, height;
	double inchWidth = GConvertXUnits(x1 - x0, NFC, INCHES, dd);
	double inchHeight = GConvertYUnits(y1 - y0, NFC, INCHES, dd);
	/* shrink the longer side */
	if (inchWidth > inchHeight) {
	    width = 0.5*GConvertXUnits(inchHeight, INCHES, NFC, dd);
	    center = 0.5*(x1 + x0);
	    x0 = center-width;
	    x1 = center+width;
	}
	else {
	    height = 0.5*GConvertYUnits(inchWidth, INCHES, NFC, dd);
	    center = 0.5*(y1 + y0);
	    y0 = center-height;
	    y1 = center+height;
	}
    }
    Rf_gpptr(dd)->plt[0] = Rf_dpptr(dd)->plt[0] = x0;
    Rf_gpptr(dd)->plt[1] = Rf_dpptr(dd)->plt[1] = x1;
    Rf_gpptr(dd)->plt[2] = Rf_dpptr(dd)->plt[2] = y0;
    Rf_gpptr(dd)->plt[3] = Rf_dpptr(dd)->plt[3] = y1;
    Rf_gpptr(dd)->pUnits = Rf_dpptr(dd)->pUnits = NFC;
}

static void updatePlotRegion(DevDesc *dd)
{
    double nfcWidth, nfcHeight;
    switch (Rf_gpptr(dd)->pUnits) {
    case NFC:
	Rf_gpptr(dd)->pin[0] = Rf_dpptr(dd)->pin[0] =
	    GConvertXUnits(Rf_gpptr(dd)->plt[1] - Rf_gpptr(dd)->plt[0], NFC, INCHES, dd);
	Rf_gpptr(dd)->pin[1] = Rf_dpptr(dd)->pin[1] =
	    GConvertYUnits(Rf_gpptr(dd)->plt[3] - Rf_gpptr(dd)->plt[2], NFC, INCHES, dd);
	break;
    case INCHES:
	nfcWidth = GConvertXUnits(Rf_gpptr(dd)->pin[0], INCHES, NFC, dd);
	nfcHeight = GConvertYUnits(Rf_gpptr(dd)->pin[1], INCHES, NFC, dd);
	Rf_gpptr(dd)->plt[0] = Rf_dpptr(dd)->plt[0] = 0.5 - nfcWidth/2;
	Rf_gpptr(dd)->plt[1] = Rf_dpptr(dd)->plt[1] = Rf_gpptr(dd)->plt[0] + nfcWidth;
	Rf_gpptr(dd)->plt[2] = Rf_dpptr(dd)->plt[2] = 0.5 - nfcHeight/2;
	Rf_gpptr(dd)->plt[3] = Rf_dpptr(dd)->plt[3] = Rf_gpptr(dd)->plt[2] + nfcHeight;
	break;
    default: /*nothing*/ break;
    }
}

/*  GMapWin2Fig -- transformation from Usr to NFC */

void GMapWin2Fig(DevDesc *dd)
{
    if (Rf_gpptr(dd)->xlog) {
	Rf_gpptr(dd)->win2fig.bx = Rf_dpptr(dd)->win2fig.bx =
	    (Rf_gpptr(dd)->plt[1] - Rf_gpptr(dd)->plt[0])/
	    (Rf_gpptr(dd)->logusr[1] - Rf_gpptr(dd)->logusr[0]);
	Rf_gpptr(dd)->win2fig.ax = Rf_dpptr(dd)->win2fig.ax =
	    Rf_gpptr(dd)->plt[0] - Rf_gpptr(dd)->win2fig.bx * Rf_gpptr(dd)->logusr[0];
    }
    else {
	Rf_gpptr(dd)->win2fig.bx = Rf_dpptr(dd)->win2fig.bx =
	    (Rf_gpptr(dd)->plt[1] - Rf_gpptr(dd)->plt[0])/
	    (Rf_gpptr(dd)->usr[1] - Rf_gpptr(dd)->usr[0]);
	Rf_gpptr(dd)->win2fig.ax = Rf_dpptr(dd)->win2fig.ax =
	    Rf_gpptr(dd)->plt[0] - Rf_gpptr(dd)->win2fig.bx * Rf_gpptr(dd)->usr[0];
    }
    if (Rf_gpptr(dd)->ylog) {
	Rf_gpptr(dd)->win2fig.by = Rf_dpptr(dd)->win2fig.by =
	    (Rf_gpptr(dd)->plt[3] - Rf_gpptr(dd)->plt[2])/
	    (Rf_gpptr(dd)->logusr[3] - Rf_gpptr(dd)->logusr[2]);
	Rf_gpptr(dd)->win2fig.ay = Rf_dpptr(dd)->win2fig.ay =
	    Rf_gpptr(dd)->plt[2] - Rf_gpptr(dd)->win2fig.by * Rf_gpptr(dd)->logusr[2];
    }
    else {
	Rf_gpptr(dd)->win2fig.by = Rf_dpptr(dd)->win2fig.by =
	    (Rf_gpptr(dd)->plt[3] - Rf_gpptr(dd)->plt[2])/
	    (Rf_gpptr(dd)->usr[3] - Rf_gpptr(dd)->usr[2]);
	Rf_gpptr(dd)->win2fig.ay = Rf_dpptr(dd)->win2fig.ay =
	    Rf_gpptr(dd)->plt[2] - Rf_gpptr(dd)->win2fig.by * Rf_gpptr(dd)->usr[2];
    }
}


/*  mapping -- Set up mappings between coordinate systems  */
/*  This is the user's interface to the mapping routines above */

static
void mapping(DevDesc *dd, int which)
{
    switch(which) {
    case 0:
	mapNDC2Dev(dd);
    case 1:
	updateOuterMargins(dd);
	mapInner2Dev(dd);
    case 2:
	if (Rf_gpptr(dd)->defaultFigure)
	    mapFigureRegion(dd);
	updateFigureRegion(dd);
	mapFig2Dev(dd);
    case 3:
	updateFigureMargins(dd);
	if (Rf_gpptr(dd)->defaultPlot)
	    mapPlotRegion(dd);
	updatePlotRegion(dd);
    }
}


/*  GReset -- Reset coordinate systems mappings and unit yardsticks */

void GReset(DevDesc *dd)
{
    /* Character extents are based on the raster size */
    Rf_gpptr(dd)->asp = Rf_gpptr(dd)->ipr[1] / Rf_gpptr(dd)->ipr[0];
    Rf_gpptr(dd)->mkh = Rf_gpptr(dd)->cra[0] * Rf_gpptr(dd)->ipr[0];

    /* Recompute Mappings */
    mapping(dd, 0);
}

/*  Is the figure region too big ? */

static Rboolean validFigureRegion(DevDesc *dd)
{
    return ((Rf_gpptr(dd)->fig[0] > 0-FLT_EPSILON) &&
	    (Rf_gpptr(dd)->fig[1] < 1+FLT_EPSILON) &&
	    (Rf_gpptr(dd)->fig[2] > 0-FLT_EPSILON) &&
	    (Rf_gpptr(dd)->fig[3] < 1+FLT_EPSILON));
}

/*  Is the figure region too small ? */

static Rboolean validOuterMargins(DevDesc *dd)
{
    return ((Rf_gpptr(dd)->fig[0] < Rf_gpptr(dd)->fig[1]) &&
	    (Rf_gpptr(dd)->fig[2] < Rf_gpptr(dd)->fig[3]));
}

/* Is the plot region too big ? */

static Rboolean validPlotRegion(DevDesc *dd)
{
    return ((Rf_gpptr(dd)->plt[0] > 0-FLT_EPSILON) &&
	    (Rf_gpptr(dd)->plt[1] < 1+FLT_EPSILON) &&
	    (Rf_gpptr(dd)->plt[2] > 0-FLT_EPSILON) &&
	    (Rf_gpptr(dd)->plt[3] < 1+FLT_EPSILON));
}

/* Is the plot region too small ? */

static Rboolean validFigureMargins(DevDesc *dd)
{
    return ((Rf_gpptr(dd)->plt[0] < Rf_gpptr(dd)->plt[1]) &&
	    (Rf_gpptr(dd)->plt[2] < Rf_gpptr(dd)->plt[3]));
}

static void invalidError(char* message, DevDesc *dd)
{
    Rf_dpptr(dd)->currentFigure -= 1;
    if (Rf_dpptr(dd)->currentFigure < 1)
	Rf_dpptr(dd)->currentFigure = Rf_dpptr(dd)->lastFigure;
    Rf_gpptr(dd)->currentFigure = Rf_dpptr(dd)->currentFigure;
    error(message);
}

/*  GNewPlot -- Begin a new plot (advance to new frame if needed)  */
DevDesc *GNewPlot(Rboolean recording)
{
    DevDesc *dd;

    /* If there are no active devices
     * check the options for a "default device".
     * If there is one, start it up. */

    if (NoDevices()) {
	SEXP defdev = GetOption(install("device"), R_NilValue);
	if (isString(defdev) && length(defdev) > 0) {
	    PROTECT(defdev = lang1(install(CHAR(STRING_ELT(defdev, 0)))));
	}
	else error("No active or default device");
	eval(defdev, R_GlobalEnv);
	UNPROTECT(1);
    }

    /* Restore Default Parameters */

    dd = CurrentDevice();
    GRestore(dd);

    /* GNewPlot always starts a new plot UNLESS the user has set
     * Rf_gpptr(dd)->new to TRUE by par(new=TRUE)
     * If Rf_gpptr(dd)->new is FALSE, we leave it that way (further GNewPlot's
     * will move on to subsequent plots)
     * If Rf_gpptr(dd)->new is TRUE, any subsequent drawing will dirty the plot
     * and reset Rf_gpptr(dd)->new to FALSE
     */
    if (!Rf_gpptr(dd)->new) {
	Rf_dpptr(dd)->currentFigure += 1;
	Rf_gpptr(dd)->currentFigure = Rf_dpptr(dd)->currentFigure;
	if (Rf_gpptr(dd)->currentFigure > Rf_gpptr(dd)->lastFigure) {
	    if (recording) {
		if (Rf_gpptr(dd)->ask) {
		    NewFrameConfirm();
		    if (NoDevices())
			error("attempt to plot on null device");
		    else
			dd = CurrentDevice();
		}
		GEinitDisplayList((GEDevDesc*) dd);
	    }
	    if (dd->newDevStruct)
		GENewPage(Rf_dpptr(dd)->bg, Rf_dpptr(dd)->gamma, (GEDevDesc*) dd);
	    else
		Rf_dpptr(dd)->newPage(dd);
	    Rf_dpptr(dd)->currentFigure = Rf_gpptr(dd)->currentFigure = 1;
	}

	GReset(dd);
	GForceClip(dd);
    }

    /* IF the division of the device into separate regions */
    /* has resulted in any invalid regions ... */
    /* IF this was a user command (i.e., we are recording) */
    /* send an error message to the command line */
    /* IF we are replaying then draw a message in the output */

#define G_ERR_MSG(msg)			\
	if (recording)			\
	    invalidError(msg, dd);	\
	else {				\
            int xpdsaved = Rf_gpptr(dd)->xpd; \
            Rf_gpptr(dd)->xpd = 2; \
	    GText(0.5,0.5,NFC, msg,	\
		  0.5,0.5,  0, dd);     \
            Rf_gpptr(dd)->xpd = xpdsaved; \
        }

    Rf_dpptr(dd)->valid = Rf_gpptr(dd)->valid = FALSE;
    if (!validOuterMargins(dd)) {
	G_ERR_MSG("Outer margins too large (fig.region too small)");
    } else if (!validFigureRegion(dd)) {
	G_ERR_MSG("Figure region too large");
    } else if (!validFigureMargins(dd)) {
	G_ERR_MSG("Figure margins too large");
    } else if (!validPlotRegion(dd)) {
	G_ERR_MSG("Plot region too large");
    } else
	Rf_dpptr(dd)->valid = Rf_gpptr(dd)->valid = TRUE;

    return dd;
}
#undef G_ERR_MSG

void GScale(double min, double max, int axis, DevDesc *dd)
{
/* GScale: used to default axis information
 *	   i.e., if user has NOT specified par(usr=...)
 */
#define EPS_FAC_1  16
#define EPS_FAC_2 100

    Rboolean swap, is_xaxis = (axis == 1 || axis == 3);
    int log, n, style;
    double temp;

    if(is_xaxis) {
	n = Rf_gpptr(dd)->lab[0];
	style = Rf_gpptr(dd)->xaxs;
	log = Rf_gpptr(dd)->xlog;
    }
    else {
	n = Rf_gpptr(dd)->lab[1];
	style = Rf_gpptr(dd)->yaxs;
	log = Rf_gpptr(dd)->ylog;
    }

    if (log) {
	min = log10(min);
	max = log10(max);
    }
    if(!R_FINITE(min) || !R_FINITE(max)) {
	warning("Nonfinite axis limits [GScale(%g,%g,%d, .); log=%d]",
		min, max, axis, log);
	if(!R_FINITE(min)) min = - .45 * DBL_MAX;
	if(!R_FINITE(max)) max = + .45 * DBL_MAX;
	/* max - min is now finite */
    }
    /* Version <= 1.2.0 had
       if (min == max)	 -- exact equality for real numbers */
    temp = fmax2(fabs(max), fabs(min));
    if(temp == 0) {/* min = max = 0 */
	min = -1;
	max =  1;
    }
    else if(fabs(max - min) < temp * EPS_FAC_1 * DBL_EPSILON) {
	temp *= (min == max) ? .4 : 1e-2;
	min -= temp;
	max += temp;
    }

    switch(style) {
    case 'r':
	temp = 0.04 * (max-min);
	min -= temp;
	max += temp;
	break;
    case 'i':
	break;
    case 's':/* FIXME --- implement  's' and 'e' axis styles ! */
    case 'e':
    default:
	error("axis style \"%c\" unimplemented", style);
    }

    if(is_xaxis) {
	if (log) {
	    Rf_gpptr(dd)->usr[0] = Rf_dpptr(dd)->usr[0] = pow(10.,min);
	    Rf_gpptr(dd)->usr[1] = Rf_dpptr(dd)->usr[1] = pow(10.,max);
	    Rf_gpptr(dd)->logusr[0] = Rf_dpptr(dd)->logusr[0] = min;
	    Rf_gpptr(dd)->logusr[1] = Rf_dpptr(dd)->logusr[1] = max;
	} else {
	    Rf_gpptr(dd)->usr[0] = Rf_dpptr(dd)->usr[0] = min;
	    Rf_gpptr(dd)->usr[1] = Rf_dpptr(dd)->usr[1] = max;
	    Rf_gpptr(dd)->logusr[0] = Rf_dpptr(dd)->logusr[0] = log10(min);
	    Rf_gpptr(dd)->logusr[1] = Rf_dpptr(dd)->logusr[1] = log10(max);
	}
    } else {
	if (log) {
	    Rf_gpptr(dd)->usr[2] = Rf_dpptr(dd)->usr[2] = pow(10.,min);
	    Rf_gpptr(dd)->usr[3] = Rf_dpptr(dd)->usr[3] = pow(10.,max);
	    Rf_gpptr(dd)->logusr[2] = Rf_dpptr(dd)->logusr[2] = min;
	    Rf_gpptr(dd)->logusr[3] = Rf_dpptr(dd)->logusr[3] = max;
	} else {
	    Rf_gpptr(dd)->usr[2] = Rf_dpptr(dd)->usr[2] = min;
	    Rf_gpptr(dd)->usr[3] = Rf_dpptr(dd)->usr[3] = max;
	    Rf_gpptr(dd)->logusr[2] = Rf_dpptr(dd)->logusr[2] = log10(min);
	    Rf_gpptr(dd)->logusr[3] = Rf_dpptr(dd)->logusr[3] = log10(max);
	}
    }

    /* ------  The following : Only computation of [xy]axp[0:2] ------- */

    /* This is not directly needed when [xy]axt = "n",
     * but may later be different in another call to axis(), e.g.:
      > plot(1, xaxt = "n");  axis(1)
     * In that case, do_axis() should do the following.
     * MM: May be we should modularize and put the following into another
     * subroutine which could be called by do_axis {when [xy]axt != 'n'} ..
     */

    swap = min > max;
    if(swap) { /* Feature: in R, something like  xlim = c(100,0)  just works */
	temp = min; min = max; max = temp;
    }

    if(log) {
	min = pow(10., min);
	max = pow(10., max);
	GLPretty(&min, &max, &n);
    }
    else GPretty(&min, &max, &n);

    if(fabs(max - min) < (temp = fmax2(fabs(max), fabs(min)))*
       EPS_FAC_2 * DBL_EPSILON) {
	/* Treat this case somewhat similar to the (min ~= max) case above */
	/* Too much accuracy here just shows machine differences */
	warning("relative range of values =%4.0f * EPS, is small (axis %d)."
		/*"to compute accurately"*/,
		fabs(max - min) / (temp*DBL_EPSILON), axis);

	/* No pretty()ing anymore */
	min = Rf_dpptr(dd)->usr[2]; /* original  (min,max) ..*/
	max = Rf_dpptr(dd)->usr[3];
	temp = .01 * fabs(max - min);
	min += temp;
	max -= temp;
	n = 1;
    }

    if(swap) {
	temp = min; min = max; max = temp;
    }

#define G_Store_AXP(is_X)			\
    if(is_X) {					\
	Rf_gpptr(dd)->xaxp[0] = Rf_dpptr(dd)->xaxp[0] = min;	\
	Rf_gpptr(dd)->xaxp[1] = Rf_dpptr(dd)->xaxp[1] = max;	\
	Rf_gpptr(dd)->xaxp[2] = Rf_dpptr(dd)->xaxp[2] = n;	\
    }						\
    else {					\
	Rf_gpptr(dd)->yaxp[0] = Rf_dpptr(dd)->yaxp[0] = min;	\
	Rf_gpptr(dd)->yaxp[1] = Rf_dpptr(dd)->yaxp[1] = max;	\
	Rf_gpptr(dd)->yaxp[2] = Rf_dpptr(dd)->yaxp[2] = n;	\
    }

    G_Store_AXP(is_xaxis);
}
#undef EPS_FAC_1
#undef EPS_FAC_2

void GSetupAxis(int axis, DevDesc *dd)
{
/*  GSetupAxis -- Set up the default axis information
 *		  called when user specifies	par(usr =...) */
/*  What should happen if			------------
 *   xlog or ylog = TRUE ? */
    double min, max;
    int n;
    Rboolean is_xaxis = (axis == 1 || axis == 3);

    if(is_xaxis) {
	n = Rf_gpptr(dd)->lab[0];
	min = Rf_gpptr(dd)->usr[0];
	max = Rf_gpptr(dd)->usr[1];
    }
    else {
	n = Rf_gpptr(dd)->lab[1];
	min = Rf_gpptr(dd)->usr[2];
	max = Rf_gpptr(dd)->usr[3];
    }

    GPretty(&min, &max, &n);

    G_Store_AXP(is_xaxis);
}
#undef G_Store_AXP

/*-------------------------------------------------------------------
 *
 *  GPAR FUNCTIONS
 *
 */


/* Set default graphics parameter values in a GPar.
 * This initialises the plot state, plus the other graphical
 * parameters that are not the responsibility of the device initialisation.

 * Typically called from  do_<dev>(.)  as  GInit(&dd->dp)
 */
void GInit(GPar *dp)
{
    dp->state = 0;

    dp->ann = TRUE;
    dp->ask = FALSE;
    dp->err = 0;
    dp->bty = 'o';

    dp->mkh = .001;/* dummy value > 0  --- FIXME : */
    /* GREset has Rf_gpptr(dd)->mkh = Rf_gpptr(dd)->cra[0] * Rf_gpptr(dd)->ipr[0]; */
    dp->cex = 1.0;
    dp->cexbase = 1.0;
    dp->cexmain = 1.2;
    dp->cexlab = 1.0;
    dp->cexsub = 1.0;
    dp->cexaxis = 1.0;

    dp->col = 0;
    dp->colmain = 0;
    dp->collab = 0;
    dp->colsub = 0;
    dp->colaxis = 0;
    dp->gamma = 1;

    /* dp->ps = 10; */	/* Device Specific */
    dp->metricInfo = 0;
    dp->font = 1;
    dp->fontmain = 2;
    dp->fontlab = 1;
    dp->fontsub = 1;
    dp->fontaxis = 1;

    dp->pch = 1;
    dp->lty = LTY_SOLID;
    dp->smo = 1;

    /* String Adjustment and rotation */
    dp->adj = 0.5;
    dp->crt = 0.0;
    dp->srt = 0.0;

    /* Positioning of margin text */
    dp->mgp[0] = 3;
    dp->mgp[1] = 1;
    dp->mgp[2] = 0;

    /* Axis annotation parameters */
    dp->lab[0] = 5;
    dp->lab[1] = 5;
    dp->lab[2] = 7;
    dp->las = 0;
    dp->tck = NA_REAL;
    dp->tcl = -0.5;
    dp->tmag = 1.2;
    dp->type = 'p';
    dp->xaxp[0] = 0.0;
    dp->xaxp[1] = 1.0;
    dp->xaxp[2] = 5.0;
    dp->xaxs = 'r';
    dp->xaxt = 's';
    dp->xlog = FALSE;
    dp->xpd = 0;
    dp->oldxpd = -99;
    dp->yaxp[0] = 0.0;
    dp->yaxp[1] = 1.0;
    dp->yaxp[2] = 5.0;
    dp->yaxs = 'r';
    dp->yaxt = 's';
    dp->ylog = FALSE;

    /* Outer Margins */
    dp->mex = 1.0;
    dp->oma[0] = 0.0;
    dp->oma[1] = 0.0;
    dp->oma[2] = 0.0;
    dp->oma[3] = 0.0;
    dp->oUnits = LINES;
    dp->fig[0] = 0.0;
    dp->fig[1] = 1.0;
    dp->fig[2] = 0.0;
    dp->fig[3] = 1.0;
    dp->fUnits = NIC;
    dp->defaultFigure = TRUE;	/* the figure region is calculated from */
				/* the layout by default */
    dp->pUnits = NFC;
    dp->defaultPlot = TRUE;	/* the plot region is calculated as */
				/* figure-margin by default */

    /* Inner Margins */
    dp->mar[0] = 5.1;
    dp->mar[1] = 4.1;
    dp->mar[2] = 4.1;
    dp->mar[3] = 2.1;
    dp->mUnits = LINES;

    /* Multi-figure parameters */
    dp->layout = FALSE;
    dp->mfind  = 0;

    dp->numrows = 1;
    dp->numcols = 1;
    dp->currentFigure = 1;
    dp->lastFigure = 1;
    dp->heights[0] = 1;
    dp->widths[0] = 1;
    dp->cmHeights[0] = 0;
    dp->cmWidths[0] = 0;
    dp->order[0][0] = 1;
    dp->rspct = 0;
    dp->respect[0][0] = 0;

    /* Misc plotting parameters */
    dp->new = FALSE;
    dp->devmode = -99;
    dp->pty = 'm';
    dp->lwd = 1;

    /* Data window */
    dp->usr[0] = 0.0;
    dp->usr[1] = 1.0;
    dp->usr[2] = 0.0;
    dp->usr[3] = 1.0;
}


/* Copy a GPar structure from source to dest. */
void copyGPar(GPar *source, GPar *dest)
{
    memcpy(dest, source, sizeof(GPar));
}


/* Restore the graphics parameters from the device copy. */
void GRestore(DevDesc *dd)
{
    if (NoDevices())
	error("No graphics device is active");
    copyGPar(Rf_dpptr(dd), Rf_gpptr(dd));
}


/*  Saving and restoring of "inline" graphical	*/
/*  parameters.	 These are the ones which can be  */
/*  specified as a arguments to high-level  */
/*  graphics functions.	 */

static double	adjsave;	/* adj */
static int	annsave;	/* ann */
static int	btysave;	/* bty */
static double	cexsave;	/* cex */
static double	cexbasesave;	/* cexbase */
static double	cexmainsave;	/* cex.main */
static double	cexlabsave;	/* cex.lab */
static double	cexsubsave;	/* cex.sub */
static double	cexaxissave;	/* cex.axis */
static int	colsave;	/* col */
static int	fgsave;		/* fg */
static int	bgsave;		/* bg */
static int	colmainsave;	/* col.main */
static int	collabsave;	/* col.lab */
static int	colsubsave;	/* col.sub */
static int	colaxissave;	/* col.axis */
static double	crtsave;	/* character rotation */
static int	fontsave;	/* font */
static int	fontmainsave;	/* font.main */
static int	fontlabsave;	/* font.lab */
static int	fontsubsave;	/* font.sub */
static int	fontaxissave;	/* font.axis */
static int	errsave;	/* error mode */
static int	labsave[3];	/* axis labelling parameters */
static int	lassave;	/* label style */
static int	ltysave;	/* line type */
static double	lwdsave;	/* line width */
static double	mgpsave[3];	/* margin position for annotation */
static double	mkhsave;	/* mark height */
static int	pchsave;	/* plotting character */
static double	srtsave;	/* string rotation */
static double	tcksave;	/* tick mark length */
static double	tclsave;	/* tick mark length in LINES */
static double	xaxpsave[3];	/* x axis parameters */
static int	xaxssave;	/* x axis calculation style */
static int	xaxtsave;	/* x axis type */
static int	xpdsave;	/* clipping control */
static double	yaxpsave[3];	/* y axis parameters */
static int	yaxssave;	/* y axis calculation style */
static int	yaxtsave;	/* y axis type */


/* Make a temporary copy of the inline parameter values. */
void GSavePars(DevDesc *dd)
{
    adjsave = Rf_gpptr(dd)->adj;
    annsave = Rf_gpptr(dd)->ann;
    btysave = Rf_gpptr(dd)->bty;
    cexsave = Rf_gpptr(dd)->cex;
    cexbasesave = Rf_gpptr(dd)->cexbase;
    cexlabsave = Rf_gpptr(dd)->cexlab;
    cexmainsave = Rf_gpptr(dd)->cexmain;
    cexsubsave = Rf_gpptr(dd)->cexsub;
    cexaxissave = Rf_gpptr(dd)->cexaxis;
    colsave = Rf_gpptr(dd)->col;
    fgsave = Rf_gpptr(dd)->fg;
    bgsave = Rf_gpptr(dd)->bg;
    collabsave = Rf_gpptr(dd)->collab;
    colmainsave = Rf_gpptr(dd)->colmain;
    colsubsave = Rf_gpptr(dd)->colsub;
    colaxissave = Rf_gpptr(dd)->colaxis;
    crtsave = Rf_gpptr(dd)->crt;
    errsave = Rf_gpptr(dd)->err;
    fontsave = Rf_gpptr(dd)->font;
    fontmainsave = Rf_gpptr(dd)->fontmain;
    fontlabsave = Rf_gpptr(dd)->fontlab;
    fontsubsave = Rf_gpptr(dd)->fontsub;
    fontaxissave = Rf_gpptr(dd)->fontaxis;
    /* csisave = Rf_gpptr(dd)->csi; */
    labsave[0] = Rf_gpptr(dd)->lab[0];
    labsave[1] = Rf_gpptr(dd)->lab[1];
    labsave[2] = Rf_gpptr(dd)->lab[2];
    lassave = Rf_gpptr(dd)->las;
    ltysave = Rf_gpptr(dd)->lty;
    lwdsave = Rf_gpptr(dd)->lwd;
    mgpsave[0] = Rf_gpptr(dd)->mgp[0];
    mgpsave[1] = Rf_gpptr(dd)->mgp[1];
    mgpsave[2] = Rf_gpptr(dd)->mgp[2];
    mkhsave = Rf_gpptr(dd)->mkh;
    pchsave = Rf_gpptr(dd)->pch;
    srtsave = Rf_gpptr(dd)->srt;
    tcksave = Rf_gpptr(dd)->tck;
    tclsave = Rf_gpptr(dd)->tcl;
    xaxpsave[0] = Rf_gpptr(dd)->xaxp[0];
    xaxpsave[1] = Rf_gpptr(dd)->xaxp[1];
    xaxpsave[2] = Rf_gpptr(dd)->xaxp[2];
    xaxssave = Rf_gpptr(dd)->xaxs;
    xaxtsave = Rf_gpptr(dd)->xaxt;
    xpdsave = Rf_gpptr(dd)->xpd;
    yaxpsave[0] = Rf_gpptr(dd)->yaxp[0];
    yaxpsave[1] = Rf_gpptr(dd)->yaxp[1];
    yaxpsave[2] = Rf_gpptr(dd)->yaxp[2];
    yaxssave = Rf_gpptr(dd)->yaxs;
    yaxtsave = Rf_gpptr(dd)->yaxt;
}


/*  Restore temporarily saved inline parameter values	*/
void GRestorePars(DevDesc *dd)
{
    Rf_gpptr(dd)->adj = adjsave;
    Rf_gpptr(dd)->ann = annsave;
    Rf_gpptr(dd)->bty = btysave;
    Rf_gpptr(dd)->cex = cexsave;
    Rf_gpptr(dd)->cexbase = cexbasesave;
    Rf_gpptr(dd)->cexlab = cexlabsave;
    Rf_gpptr(dd)->cexmain = cexmainsave;
    Rf_gpptr(dd)->cexsub = cexsubsave;
    Rf_gpptr(dd)->cexaxis = cexaxissave;
    Rf_gpptr(dd)->col = colsave;
    Rf_gpptr(dd)->fg = fgsave;
    Rf_gpptr(dd)->bg = bgsave;
    Rf_gpptr(dd)->collab = collabsave;
    Rf_gpptr(dd)->colmain = colmainsave;
    Rf_gpptr(dd)->colsub = colsubsave;
    Rf_gpptr(dd)->colaxis = colaxissave;
    Rf_gpptr(dd)->crt = crtsave;
    Rf_gpptr(dd)->err = errsave;
    Rf_gpptr(dd)->font = fontsave;
    Rf_gpptr(dd)->fontmain = fontmainsave;
    Rf_gpptr(dd)->fontlab = fontlabsave;
    Rf_gpptr(dd)->fontsub = fontsubsave;
    Rf_gpptr(dd)->fontaxis = fontaxissave;
    /* Rf_gpptr(dd)->csi = csisave; */
    Rf_gpptr(dd)->lab[0] = labsave[0];
    Rf_gpptr(dd)->lab[1] = labsave[1];
    Rf_gpptr(dd)->lab[2] = labsave[2];
    Rf_gpptr(dd)->las = lassave;
    Rf_gpptr(dd)->lty = ltysave;
    Rf_gpptr(dd)->lwd = lwdsave;
    Rf_gpptr(dd)->mgp[0] = mgpsave[0];
    Rf_gpptr(dd)->mgp[1] = mgpsave[1];
    Rf_gpptr(dd)->mgp[2] = mgpsave[2];
    Rf_gpptr(dd)->mkh = mkhsave;
    Rf_gpptr(dd)->pch = pchsave;
    Rf_gpptr(dd)->srt = srtsave;
    Rf_gpptr(dd)->tck = tcksave;
    Rf_gpptr(dd)->tcl = tclsave;
    Rf_gpptr(dd)->xaxp[0] = xaxpsave[0];
    Rf_gpptr(dd)->xaxp[1] = xaxpsave[1];
    Rf_gpptr(dd)->xaxp[2] = xaxpsave[2];
    Rf_gpptr(dd)->xaxs = xaxssave;
    Rf_gpptr(dd)->xaxt = xaxtsave;
    Rf_gpptr(dd)->xpd = xpdsave;
    Rf_gpptr(dd)->yaxp[0] = yaxpsave[0];
    Rf_gpptr(dd)->yaxp[1] = yaxpsave[1];
    Rf_gpptr(dd)->yaxp[2] = yaxpsave[2];
    Rf_gpptr(dd)->yaxs = yaxssave;
    Rf_gpptr(dd)->yaxt = yaxtsave;
}

/*-------------------------------------------------------------------
 *
 *  DEVICE STATE FUNCTIONS
 *
 */


/* This records whether GNewPlot has been called. */
void GSetState(int newstate, DevDesc *dd)
{
    Rf_dpptr(dd)->state = Rf_gpptr(dd)->state = newstate;
}



/* Enquire whether GNewPlot has been called. */
void GCheckState(DevDesc *dd)
{
    if(Rf_gpptr(dd)->state == 0)
	error("plot.new has not been called yet");
    if (!Rf_gpptr(dd)->valid)
#ifdef OLD
	onintr();
#else
        error("invalid graphics state");
#endif
}

/*-------------------------------------------------------------------
 * GRAPHICAL PRIMITIVES
 *
 */

/* CLIPPING paradigm:

   R uses both the clipping capabilities of the device (if present)
   and its own internal clipping algorithms.
   If the device has no clipping capabilities (canClip = FALSE) then R
   does all of the clipping internally.
   If the device has clipping capabilities, R still does some internal
   clipping (to the device extent).  This is to avoid "silly" values
   being sent to the device (e.g., X11 and Ghostview will barf if you
   send a ridiculously large number to them).  Call this silly-clipping.

       The problem with getting R to do some of the clipping is that it is
       not necessarily as good as the device at clipping (e.g., R's text
       clipping is very crude).  This is the motivation for leaving as much
       of the clipping as possible to the device.
       R does different amounts of silly-clipping for different primitives.
       See the individual routines for more info.
*/


static void setClipRect(double *x1, double *y1, double *x2, double *y2,
                        int coords, DevDesc *dd)
{
    /*
     * xpd = 0 means clip to current plot region
     * xpd = 1 means clip to current figure region
     * xpd = 2 means clip to device region
     */
    *x1 = 0.0;
    *y1 = 0.0;
    *x2 = 1.0;
    *y2 = 1.0;
    switch (Rf_gpptr(dd)->xpd) {
    case 0:
	GConvert(x1, y1, NPC, coords, dd);
	GConvert(x2, y2, NPC, coords, dd);
	break;
    case 1:
	GConvert(x1, y1, NFC, coords, dd);
	GConvert(x2, y2, NFC, coords, dd);
	break;
    case 2:
	GConvert(x1, y1, NDC, coords, dd);
	GConvert(x2, y2, NDC, coords, dd);
	break;
    }
}

/* Update the device clipping region (depends on GP->xpd). */
void GClip(DevDesc *dd)
{
    if (Rf_gpptr(dd)->xpd != Rf_gpptr(dd)->oldxpd) {
	double x1, y1, x2, y2;
	setClipRect(&x1, &y1, &x2, &y2, DEVICE, dd);
	GESetClip(x1, y1, x2, y2, (GEDevDesc*) dd);
	Rf_gpptr(dd)->oldxpd = Rf_gpptr(dd)->xpd;
    }
}


/*  Forced update of the device clipping region. */
void GForceClip(DevDesc *dd)
{
    double x1, y1, x2, y2;
    if (Rf_gpptr(dd)->state == 0) return;
    setClipRect(&x1, &y1, &x2, &y2, DEVICE, dd);
    GESetClip(x1, y1, x2, y2, (GEDevDesc*) dd);
}


/* Draw Line Segments, Clipping to the Viewport */
/* Cohen-Sutherland Algorithm */
/* Unneeded if the device can do the clipping */


#define	CS_BOTTOM	001
#define	CS_LEFT		002
#define	CS_TOP		004
#define	CS_RIGHT	010

typedef struct {
    double xl;
    double xr;
    double yb;
    double yt;
} cliprect;


static int clipcode(double x, double y, cliprect *cr)
{
    int c = 0;
    if(x < cr->xl)
	c |= CS_LEFT;
    else if(x > cr->xr)
	c |= CS_RIGHT;
    if(y < cr->yb)
	c |= CS_BOTTOM;
    else if(y > cr->yt)
	c |= CS_TOP;
    return c;
}

static Rboolean
CSclipline(double *x1, double *y1, double *x2, double *y2, cliprect *cr,
	   int *clipped1, int *clipped2, int coords, DevDesc *dd)
{
    int c, c1, c2;
    double x, y, xl, xr, yb, yt;
    cliprect cr2;

    *clipped1 = 0;
    *clipped2 = 0;
    c1 = clipcode(*x1, *y1, cr);
    c2 = clipcode(*x2, *y2, cr);
    if ( !c1 && !c2 )
	return TRUE;

    xl = cr->xl;
    xr = cr->xr;
    yb = cr->yb;
    yt = cr->yt;
    if (Rf_gpptr(dd)->xlog || Rf_gpptr(dd)->ylog) {
	double temp;

	GConvert(x1, y1, coords, NDC, dd);
	GConvert(x2, y2, coords, NDC, dd);
	GConvert(&xl, &yb, coords, NDC, dd);
	GConvert(&xr, &yt, coords, NDC, dd);

	cr2.xl = xl;
	cr2.xr = xr;
	cr2.yb = yb;
	cr2.yt = yt;

        if (cr2.xr < cr2.xl) {
            temp = cr2.xl;
            cr2.xl = cr2.xr;
            cr2.xr = temp;
        }
        if (cr2.yt < cr2.yb) {
            temp = cr2.yb;
            cr2.yb = cr2.yt;
            cr2.yt = temp;
        }

	x = xl;		/* keep -Wall happy */
	y = yb;		/* keep -Wall happy */
	while( c1 || c2 ) {
	    if(c1 & c2)
		return FALSE;
	    if( c1 )
		c = c1;
	    else
		c = c2;
	    if( c & CS_LEFT ) {
		y = *y1 + (*y2 - *y1) * (xl - *x1) / (*x2 - *x1);
		x = xl;
	    }
	    else if( c & CS_RIGHT ) {
		y = *y1 + (*y2 - *y1) * (xr - *x1) / (*x2 -  *x1);
		x = xr;
	    }
	    else if( c & CS_BOTTOM ) {
		x = *x1 + (*x2 - *x1) * (yb - *y1) / (*y2 - *y1);
		y = yb;
	    }
	    else if( c & CS_TOP ) {
		x = *x1 + (*x2 - *x1) * (yt - *y1)/(*y2 - *y1);
		y = yt;
	    }

	    if( c==c1 ) {
		*x1 = x;
		*y1 = y;
		*clipped1 = 1;
		c1 = clipcode(x, y, &cr2);
	    }
	    else {
		*x2 = x;
		*y2 = y;
		*clipped2 = 1;
		c2 = clipcode(x, y, &cr2);
	    }
	}

	GConvert(x1, y1, NDC, coords, dd);
	GConvert(x2, y2, NDC, coords, dd);

    } else {
	x = xl;		/* keep -Wall happy */
	y = yb;		/* keep -Wall happy */
	while( c1 || c2 ) {
	    if(c1 & c2)
		return FALSE;
	    if( c1 )
		c = c1;
	    else
		c = c2;
	    if( c & CS_LEFT ) {
		y = *y1 + (*y2 - *y1) * (xl - *x1) / (*x2 - *x1);
		x = xl;
	    }
	    else if( c & CS_RIGHT ) {
		y = *y1 + (*y2 - *y1) * (xr - *x1) / (*x2 -  *x1);
		x = xr;
	    }
	    else if( c & CS_BOTTOM ) {
		x = *x1 + (*x2 - *x1) * (yb - *y1) / (*y2 - *y1);
		y = yb;
	    }
	    else if( c & CS_TOP ) {
		x = *x1 + (*x2 - *x1) * (yt - *y1)/(*y2 - *y1);
		y = yt;
	    }

	    if( c==c1 ) {
		*x1 = x;
		*y1 = y;
		*clipped1 = 1;
		c1 = clipcode(x, y, cr);
	    }
	    else {
		*x2 = x;
		*y2 = y;
		*clipped2 = 1;
		c2 = clipcode(x, y, cr);
	    }
	}
    }
    return TRUE;
}


static void CScliplines(int n, double *x, double *y, int coords, DevDesc *dd)
{
    int ind1, ind2;
    /*int firstPoint = 1;*/
    int count = 0;
    int i = 0;
    double *xx, *yy, temp;
    double x1, y1, x2, y2;
    cliprect cr;
    char *vmax = vmaxget();

    /* Work in device coordinates because then it is much easier
     * to cope with both old and new devices.
     */
    setClipRect(&cr.xl, &cr.yb, &cr.xr, &cr.yt, DEVICE, dd);
    if (cr.xr < cr.xl) {
	temp = cr.xl;
	cr.xl = cr.xr;
	cr.xr = temp;
    }
    if (cr.yt < cr.yb) {
	temp = cr.yb;
	cr.yb = cr.yt;
	cr.yt = temp;
    }

    xx = (double *) R_alloc(n, sizeof(double));
    yy = (double *) R_alloc(n, sizeof(double));
    if (xx == NULL || yy == NULL)
	error("out of memory while clipping polyline");

    x1 = x[0];
    y1 = y[0];
    GConvert(&x1, &y1, coords, DEVICE, dd);
    xx[0] = x1;
    yy[0] = y1;
    count = 1;

    for (i = 1; i < n; i++) {
	x2 = x[i];
	y2 = y[i];
	GConvert(&x2, &y2, coords, DEVICE, dd);
	if (CSclipline(&x1, &y1, &x2, &y2, &cr, &ind1, &ind2, DEVICE, dd)) {
	    if (ind1 && ind2) {
		xx[0] = x1;
		yy[0] = y1;
		xx[1] = x2;
		yy[1] = y2;
		if (dd->newDevStruct)
		    ((GEDevDesc*) dd)->dev->polyline(2, xx, yy,
						     Rf_gpptr(dd)->col,
						     Rf_gpptr(dd)->gamma,
						     Rf_gpptr(dd)->lty,
						     Rf_gpptr(dd)->lwd,
						     ((GEDevDesc*) dd)->dev);
		else
		    Rf_dpptr(dd)->polyline(2, xx, yy, DEVICE, dd);
	    }
	    else if (ind1) {
		xx[0] = x1;
		yy[0] = y1;
		xx[1] = x2;
		yy[1] = y2;
		count = 2;
		if (i == n - 1) {
		    if (dd->newDevStruct)
			((GEDevDesc*) dd)->dev->polyline(count, xx, yy,
							 Rf_gpptr(dd)->col,
							 Rf_gpptr(dd)->gamma,
							 Rf_gpptr(dd)->lty,
							 Rf_gpptr(dd)->lwd,
							 ((GEDevDesc*) dd)->dev);
		    else
			Rf_dpptr(dd)->polyline(count, xx, yy, DEVICE, dd);
		}
	    }
	    else if (ind2) {
		xx[count] = x2;
		yy[count] = y2;
		count++;
		if (count > 1) {
		    if (dd->newDevStruct)
			((GEDevDesc*) dd)->dev->polyline(count, xx, yy,
							 Rf_gpptr(dd)->col,
							 Rf_gpptr(dd)->gamma,
							 Rf_gpptr(dd)->lty,
							 Rf_gpptr(dd)->lwd,
							 ((GEDevDesc*) dd)->dev);
		    else
			Rf_dpptr(dd)->polyline(count, xx, yy, DEVICE, dd);
		}
	    }
	    else {
		xx[count] = x2;
		yy[count] = y2;
		count++;
		if (i == n - 1 && count > 1) {
		    if (dd->newDevStruct)
			((GEDevDesc*) dd)->dev->polyline(count, xx, yy,
							 Rf_gpptr(dd)->col,
							 Rf_gpptr(dd)->gamma,
							 Rf_gpptr(dd)->lty,
							 Rf_gpptr(dd)->lwd,
							 ((GEDevDesc*) dd)->dev);
		    else
			Rf_dpptr(dd)->polyline(count, xx, yy, DEVICE, dd);
		}
	    }
	}
	x1 = x[i];
	y1 = y[i];
	GConvert(&x1, &y1, coords, DEVICE, dd);
    }

    vmaxset(vmax);
}

/* Clip the line
   If toDevice = 1, clip to the device extent (i.e., temporarily ignore
   Rf_gpptr(dd)->xpd) */
static Rboolean
clipLine(double *x1, double *y1, double *x2, double *y2,
	 int coords, int toDevice, DevDesc *dd)
{
    double temp;
    int dummy1, dummy2;
    int xpdsaved = 0; /* -Wall */
    Rboolean result;
    cliprect cr;

    if (toDevice) {
	xpdsaved = Rf_gpptr(dd)->xpd;
	Rf_gpptr(dd)->xpd = 2;
    }

    setClipRect(&cr.xl, &cr.yb, &cr.xr, &cr.yt, coords, dd);
    if (cr.xr < cr.xl) {
	temp = cr.xl;
	cr.xl = cr.xr;
	cr.xr = temp;
    }
    if (cr.yt < cr.yb) {
	temp = cr.yb;
	cr.yb = cr.yt;
	cr.yt = temp;
    }

    result = CSclipline(x1, y1, x2, y2, &cr, &dummy1, &dummy2, coords, dd);

    if (toDevice)
	Rf_gpptr(dd)->xpd = xpdsaved;
    return result;
}

/* Draw a line. */
/* If the device canClip, R clips line to device extent and
   device does all other clipping. */
void GLine(double x1, double y1, double x2, double y2, int coords, DevDesc *dd)
{
    Rboolean clip_ok;
    if (Rf_gpptr(dd)->lty == LTY_BLANK) return;
    if (Rf_dpptr(dd)->canClip) {
	GClip(dd);
	clip_ok = clipLine(&x1, &y1, &x2, &y2, coords, 1, dd);
    }
    else {
	clip_ok = clipLine(&x1, &y1, &x2, &y2, coords, 0, dd);
    }
    if (clip_ok) {
	if (dd->newDevStruct) {
	    GConvert(&x1, &y1, coords, DEVICE, dd);
	    GConvert(&x2, &y2, coords, DEVICE, dd);
	    ((GEDevDesc*) dd)->dev->line(x1, y1, x2, y2,
					 Rf_gpptr(dd)->col,
					 Rf_gpptr(dd)->gamma,
					 Rf_gpptr(dd)->lty,
					 Rf_gpptr(dd)->lwd,
					 ((GEDevDesc*) dd)->dev);
	} else
	    Rf_dpptr(dd)->line(x1, y1, x2, y2, coords, dd);
    }
}

/* Read the current "pen" position. */
Rboolean GLocator(double *x, double *y, int coords, DevDesc *dd)
{
    if (dd->newDevStruct) {
	if(!((GEDevDesc*) dd)->dev->locator)
	    error("no locator capability in device driver");
	if(((GEDevDesc*) dd)->dev->locator(x, y, ((GEDevDesc*) dd)->dev)) {
	    GConvert(x, y, DEVICE, coords, dd);
	    return TRUE;
	}
	else
	    return FALSE;
    } else {
	if(!Rf_dpptr(dd)->locator)
	    error("no locator capability in device driver");
	if(Rf_dpptr(dd)->locator(x, y, dd)) {
	    GConvert(x, y, DEVICE, coords, dd);
	    return TRUE;
	}
	else
	    return FALSE;
    }
}

/* Access character font metric information.  */
void GMetricInfo(int c, double *ascent, double *descent, double *width,
		 GUnit units, DevDesc *dd)
{
    if (dd->newDevStruct)
	((GEDevDesc*) dd)->dev->metricInfo(c & 0xFF, Rf_gpptr(dd)->font,
					   Rf_gpptr(dd)->cex,
					   (double) Rf_gpptr(dd)->ps,
					   ascent, descent, width,
					   ((GEDevDesc*) dd)->dev);
    else
	Rf_dpptr(dd)->metricInfo(c & 0xFF, ascent, descent, width, dd);
    if (units != DEVICE) {
	*ascent = GConvertYUnits(*ascent, DEVICE, units, dd);
	*descent = GConvertYUnits(*descent, DEVICE, units, dd);
	*width = GConvertXUnits(*width, DEVICE, units, dd);
    }
}


/* Check that everything is initialized :
  	Interpretation :
  	mode = 0, graphics off
  	mode = 1, graphics on
  	mode = 2, graphical input on (ignored by most drivers)
*/
void GMode(int mode, DevDesc *dd)
{
    if (NoDevices())
	error("No graphics device is active");
    if(mode != Rf_gpptr(dd)->devmode) {
	if (dd->newDevStruct)
	    ((GEDevDesc*) dd)->dev->mode(mode, ((GEDevDesc*) dd)->dev);
	else
	    Rf_dpptr(dd)->mode(mode, dd);
    }
    Rf_gpptr(dd)->new = Rf_dpptr(dd)->new = FALSE;
    Rf_gpptr(dd)->devmode = Rf_dpptr(dd)->devmode = mode;
}


/* GPolygon -- Draw a polygon
 *	Filled with color bg and outlined with color fg
 *	These may both be NA_INTEGER
 * If device can't clip we should use something like Sutherland-Hodgman here
 */
typedef enum {
    Left = 0,
    Right = 1,
    Bottom = 2,
    Top = 3
} Edge;

/* Clipper State Variables */
typedef struct {
    int first;    /* true if we have seen the first point */
    double fx;    /* x coord of the first point */
    double fy;    /* y coord of the first point */
    double sx;    /* x coord of the most recent point */
    double sy;    /* y coord of the most recent point */
}
GClipState;

/* The Clipping Rectangle */
typedef struct {
    double xmin;
    double xmax;
    double ymin;
    double ymax;
}
GClipRect;

static
int inside (Edge b, double px, double py, GClipRect *clip)
{
    switch (b) {
    case Left:   if (px < clip->xmin) return 0; break;
    case Right:  if (px > clip->xmax) return 0; break;
    case Bottom: if (py < clip->ymin) return 0; break;
    case Top:    if (py > clip->ymax) return 0; break;
    }
    return 1;
}

static
int cross (Edge b, double x1, double y1, double x2, double y2,
	   GClipRect *clip)
{
    if (inside (b, x1, y1, clip) == inside (b, x2, y2, clip))
	return 0;
    else return 1;
}

static
void intersect (Edge b, double x1, double y1, double x2, double y2,
		double *ix, double *iy, GClipRect *clip)
{
    double m = 0;

    if (x1 != x2) m = (y1 - y2) / (x1 - x2);
    switch (b) {
    case Left:
	*ix = clip->xmin;
	*iy = y2 + (clip->xmin - x2) * m;
	break;
    case Right:
	*ix = clip->xmax;
	*iy = y2 + (clip->xmax - x2) * m;
	break;
    case Bottom:
	*iy = clip->ymin;
	if (x1 != x2) *ix = x2 + (clip->ymin - y2) / m;
	else *ix = x2;
	break;
    case Top:
	*iy = clip->ymax;
	if (x1 != x2) *ix = x2 + (clip->ymax - y2) / m;
	else *ix = x2;
	break;
    }
}

static
void clipPoint (Edge b, double x, double y,
		double *xout, double *yout, int *cnt, int store,
		GClipRect *clip, GClipState *cs)
{
    double ix, iy;

    if (!cs[b].first) {
	/* No previous point exists for this edge. */
	/* Save this point. */
	cs[b].first = 1;
	cs[b].fx = x;
	cs[b].fy = y;
    }
    else
	/* A previous point exists.  */
	/* If 'p' and previous point cross edge, find intersection.  */
	/* Clip against next boundary, if any.  */
	/* If no more edges, add intersection to output list. */
	if (cross (b, x, y, cs[b].sx, cs[b].sy, clip)) {
	    intersect (b, x, y, cs[b].sx, cs[b].sy, &ix, &iy, clip);
	    if (b < Top)
		clipPoint (b + 1, ix, iy, xout, yout, cnt, store,
			   clip, cs);
	    else {
		if (store) {
		    xout[*cnt] = ix;
		    yout[*cnt] = iy;
		}
		(*cnt)++;
	    }
	}

    /* Save as most recent point for this edge */
    cs[b].sx = x;
    cs[b].sy = y;

    /* For all, if point is 'inside' */
    /* proceed to next clip edge, if any */
    if (inside (b, x, y, clip)) {
	if (b < Top)
	    clipPoint (b + 1, x, y, xout, yout, cnt, store, clip, cs);
	else {
	    if (store) {
		xout[*cnt] = x;
		yout[*cnt] = y;
	    }
	    (*cnt)++;
	}
    }
}

static
void closeClip (double *xout, double *yout, int *cnt, int store,
		GClipRect *clip, GClipState *cs)
{
    double ix, iy;
    Edge b;

    for (b = Left; b <= Top; b++) {
	if (cross (b, cs[b].sx, cs[b].sy, cs[b].fx, cs[b].fy, clip)) {
	    intersect (b, cs[b].sx, cs[b].sy,
		       cs[b].fx, cs[b].fy, &ix, &iy, clip);
	    if (b < Top)
		clipPoint (b + 1, ix, iy, xout, yout, cnt, store, clip, cs);
	    else {
		if (store) {
		    xout[*cnt] = ix;
		    yout[*cnt] = iy;
		}
		(*cnt)++;
	    }
	}
    }
}

int GClipPolygon(double *x, double *y, int n, int coords, int store,
		 double *xout, double *yout, DevDesc *dd)
{
    int i, cnt = 0;
    GClipState cs[4];
    GClipRect clip;
    for (i = 0; i < 4; i++)
	cs[i].first = 0;
    /* Set up the cliprect here for R. */
    setClipRect(&clip.xmin, &clip.ymin, &clip.xmax, &clip.ymax, coords, dd);
    /* If necessary, swap the clip region extremes */
    if (clip.xmax < clip.xmin) {
	double swap = clip.xmax;
	clip.xmax = clip.xmin;
	clip.xmin = swap;
    }
    if (clip.ymax < clip.ymin) {
	double swap = clip.ymax;
	clip.ymax = clip.ymin;
	clip.ymin = swap;
    }
    for (i = 0; i < n; i++)
	clipPoint (Left, x[i], y[i], xout, yout, &cnt, store, &clip, cs);
    closeClip (xout, yout, &cnt, store, &clip, cs);
    return (cnt);
}

static void clipPolygon(int n, double *x, double *y, int coords,
                        int bg, int fg, int mode, DevDesc *dd)
{
/* If mode = 0, clip according to Rf_gpptr(dd)->xpd
   If mode = 1, clip to the device extent */
#ifdef MALLOC_AWAY
    static double *xc = NULL, *yc = NULL;
#endif
    double *xc, *yc;
#ifdef MALLOC_AWAY
    double *tmp;
    if (xc != NULL) {tmp = xc; xc = NULL; free(tmp);}
    if (yc != NULL) {tmp = yc; yc = NULL; free(tmp);}
#endif
    /* if bg not specified then draw as polyline rather than polygon
     * to avoid drawing line along border of clipping region */
    if (bg == NA_INTEGER) {
	int i;
#ifdef MALLOC_AWAY
	xc = (double*)malloc((n + 1) * sizeof(double));
	yc = (double*)malloc((n + 1) * sizeof(double));
#endif
	xc = (double*) R_alloc(n + 1, sizeof(double));
	yc = (double*) R_alloc(n + 1, sizeof(double));
	for (i=0; i<n; i++) {
	    xc[i] = x[i];
	    yc[i] = y[i];
	}
	xc[n] = x[0];
	yc[n] = y[0];
	Rf_gpptr(dd)->col = fg;
	GPolyline(n+1, xc, yc, coords, dd);
    }
    else {
	int npts;
	int xpdsaved = 0; /* -Wall */
	if (mode == 1) {
	    xpdsaved = Rf_gpptr(dd)->xpd;
	    Rf_gpptr(dd)->xpd = 2;
	}
	xc = yc = 0;		/* -Wall */
	npts = GClipPolygon(x, y, n, coords, 0, xc, yc, dd);
	if (npts > 1) {
#ifdef MALLOC_AWAY
	    xc = (double*)malloc(npts * sizeof(double));
	    yc = (double*)malloc(npts * sizeof(double));
#endif
	    xc = (double*) R_alloc(npts, sizeof(double));
	    yc = (double*) R_alloc(npts, sizeof(double));
	    npts = GClipPolygon(x, y, n, coords, 1, xc, yc, dd);
	    if (dd->newDevStruct)
		((GEDevDesc*) dd)->dev->polygon(npts, xc, yc,
						fg, bg, Rf_gpptr(dd)->gamma,
						Rf_gpptr(dd)->lty, Rf_gpptr(dd)->lwd,
						((GEDevDesc*) dd)->dev);
	    else
		Rf_dpptr(dd)->polygon(npts, xc, yc, coords, bg, fg, dd);
	}
	if (mode == 1)
	    Rf_gpptr(dd)->xpd = xpdsaved;
    }
#ifdef MALLOC_AWAY
    if (xc != NULL) {tmp = xc; xc = NULL; free(tmp);}
    if (yc != NULL) {tmp = yc; yc = NULL; free(tmp);}
#endif
}

void GPolygon(int n, double *x, double *y, int coords,
	      int bg, int fg, DevDesc *dd)
{
    int i;
    double *xx;
    double *yy;
    char *vmaxsave = vmaxget();

    if (Rf_gpptr(dd)->lty == LTY_BLANK)
	fg = NA_INTEGER; /* transparent for the border */

    /* Work in device coordinates because then it is easier to
     * work with both old and new devices.
     */
    xx = (double*) R_alloc(n, sizeof(double));
    yy = (double*) R_alloc(n, sizeof(double));
    if (!xx || !yy)
	error("unable to allocate memory (in GPolyline)");
    for (i=0; i<n; i++) {
	xx[i] = x[i];
	yy[i] = y[i];
	GConvert(&(xx[i]), &(yy[i]), coords, DEVICE, dd);
    }
    if (Rf_dpptr(dd)->canClip) {
	GClip(dd);
	clipPolygon(n, xx, yy, DEVICE, bg, fg, 1, dd);
    }
    else
	clipPolygon(n, xx, yy, DEVICE, bg, fg, 0, dd);
    vmaxset(vmaxsave);
}

#include <stdio.h>

/* Clip and draw the polyline.
   If clipToDevice = 0, clip according to Rf_gpptr(dd)->xpd
   If clipToDevice = 1, clip to the device extent */
static void clipPolyline(int n, double *x, double *y, int coords,
			 int clipToDevice, DevDesc *dd)
{
    int xpdsaved = 0; /* -Wall */
    if (clipToDevice) {
	xpdsaved = Rf_gpptr(dd)->xpd;
	Rf_gpptr(dd)->xpd = 2;
    }
    CScliplines(n, x, y, coords, dd);
    if (clipToDevice)
	Rf_gpptr(dd)->xpd = xpdsaved;
}

/* Draw a series of line segments. */
/* If the device canClip, R clips to the device extent and the device
   does all other clipping */
void GPolyline(int n, double *x, double *y, int coords, DevDesc *dd)
{
    if (Rf_gpptr(dd)->lty == LTY_BLANK) return;
    if (Rf_dpptr(dd)->canClip) {
	GClip(dd); /* sets up the device clipping */
	clipPolyline(n, x, y, coords, 1, dd);  /* clips to device extent
						  then draws */
    }
    else
	clipPolyline(n, x, y, coords, 0, dd);
}

/* Convert a circle into a polygon with specified number of vertices */
/* NOTE!!!: Coordinates and circle radius are in device units here. */
static void convertCircle(double x, double y, double r,
			  int numVertices, double *xc, double *yc,
			  DevDesc *dd)
{
    int i;
    double theta = 2*M_PI/numVertices;
    for (i=0; i<numVertices; i++) {
	xc[i] = x + r*sin(theta*i);
	yc[i] = y + r*cos(theta*i);
    }
    xc[numVertices] = x;
    yc[numVertices] = y+r;
}

/* Takes a specification of a circle as input and returns a code indicating
   how the circle should be clipped.
   The return value will be -1 if the circle is to
   be totally clipped out of existence, -2 if the circle is to be
   totally left alone, 0 and above if the circle has been converted
   into a polygon (in which case, the return value indicates the
   number of vertices of the polygon and the function convertCircle()
   should be called to obtain the vertices of the polygon). */
static int clipCircleCode(double x, double y, int coords, double r,
			  DevDesc *dd)
{
    int result;
    /* determine clipping region */
    double xmin, xmax, ymin, ymax;
    setClipRect(&xmin, &ymin, &xmax, &ymax, DEVICE, dd);

    if (xmax < xmin) {
	double swap = xmax;
	xmax = xmin;
	xmin = swap;
    }
    if (ymax < ymin) {
	double swap = ymax;
	ymax = ymin;
	ymin = swap;
    }
    /* convert x & y to DEVICE (radius already DEVICE) */
    GConvert(&x, &y, coords, DEVICE, dd);
    /* if circle is all within clipping rect */
    if (x-r > xmin && x+r < xmax && y-r > ymin && y+r < ymax) {
	result = -2;
    }
    /* if circle is all outside clipping rect */
    else {
	double distance = r*r;
	if (x-r > xmax || x+r < xmin || y-r > ymax || y+r < ymin ||
            (x < xmin && y < ymin &&
             ((x-xmin)*(x-xmin)+(y-ymin)*(y-ymin) > distance)) ||
	    (x > xmax && y < ymin &&
             ((x-xmax)*(x-xmax)+(y-ymin)*(y-ymin) > distance)) ||
	    (x < xmin && y > ymax &&
	     ((x-xmin)*(x-xmin)+(y-ymax)*(y-ymax) > distance)) ||
	    (x > xmax && y > ymax &&
	     ((x-xmax)*(x-xmax)+(y-ymax)*(y-ymax) > distance))) {
	    result = -1;
	}
        /* otherwise, convert circle to polygon */
	else {
	    /* Replace circle with polygon.

	       Heuristic for number of vertices is to use theta so
	       that cos(theta)*r ~ r - 1 in device units. This is
	       roughly const * sqrt(r) so there'd be little point in
	       enforcing an upper limit. */

	    result = (r <= 6) ? 10 : 2 * M_PI/acos(1 - 1/r) ;
	}
    }
    return result;
}

/* static void clipCircle(double x, double y, int coords,
   double r, int bg, int fg, int clipToDevice, DevDesc *dd) */

void GCircle(double x, double y, int coords,
	     double radius, int bg, int fg, DevDesc *dd)
{
    double ir;
    char *vmax;
    double *xc, *yc;
    int result, result2;
    int xpdsaved = Rf_gpptr(dd)->xpd;
    ir = radius/Rf_gpptr(dd)->ipr[0];
    ir = (ir > 0) ? ir : 1;

    if (Rf_gpptr(dd)->lty == LTY_BLANK)
	fg = NA_INTEGER; /* transparent for the border */

    /* Work in device coordinates because then it is easier to
     * work with both old and new devices.
     */
    GConvert(&x, &y, coords, DEVICE, dd);
    result = clipCircleCode(x, y, DEVICE, ir, dd);

    switch (result) {
    case -2: /* No clipping;  draw all of circle */
	if (Rf_dpptr(dd)->canClip)
	    GClip(dd);
	if (dd->newDevStruct)
	    ((GEDevDesc*) dd)->dev->circle(x, y, ir, fg, bg, Rf_gpptr(dd)->gamma,
					   Rf_gpptr(dd)->lty, Rf_gpptr(dd)->lwd,
					   ((GEDevDesc*) dd)->dev);
	else
	    Rf_dpptr(dd)->circle(x, y, DEVICE, ir, bg, fg, dd);
	break;
    case -1: /* Total clipping; draw nothing */
	break;
    default: /* At this point we have figured out that the circle
	      * has intersected with the current clipping region.
	      */
	Rf_gpptr(dd)->xpd = 2;
	result2 = clipCircleCode(x, y, DEVICE, ir, dd);
	Rf_gpptr(dd)->xpd = xpdsaved;
	/* If we are still within the confines of the device,
	 * and the device can clip, then we can still draw a circle.
	 */
	if (Rf_dpptr(dd)->canClip && result2 == -2) {
	    GClip(dd);
	    if (dd->newDevStruct)
		((GEDevDesc*) dd)->dev->circle(x, y, ir, fg, bg,
					       Rf_gpptr(dd)->gamma,
					       Rf_gpptr(dd)->lty, Rf_gpptr(dd)->lwd,
					       ((GEDevDesc*) dd)->dev);
	    else
		Rf_dpptr(dd)->circle(x, y, DEVICE, ir, bg, fg, dd);
	}
	/* Otherwise, we have to fake a circle using a polyline or
	 * polygon.
	 */
	else {
	    vmax = vmaxget();
	    xc = (double*)R_alloc(result+1, sizeof(double));
	    yc = (double*)R_alloc(result+1, sizeof(double));
	    convertCircle(x, y, ir, result, xc, yc, dd);
	    if (bg == NA_INTEGER) {
		Rf_gpptr(dd)->col = fg;
		GPolyline(result+1, xc, yc, DEVICE, dd);
	    }
	    else {
		int npts;
		double *xcc, *ycc;
		xcc = ycc = 0;	/* -Wall */
		if (Rf_dpptr(dd)->canClip)
		    GClip(dd);
		npts = GClipPolygon(xc, yc, result, DEVICE,
				    0, xcc, ycc, dd);
		if (npts > 1) {
		    xcc = (double*)R_alloc(npts, sizeof(double));
		    ycc = (double*)R_alloc(npts, sizeof(double));
		    npts = GClipPolygon(xc, yc, result, DEVICE, 1,
					xcc, ycc, dd);
		    if (dd->newDevStruct)
			((GEDevDesc*) dd)->dev->polygon(npts, xcc, ycc,
							fg, bg,
							Rf_gpptr(dd)->gamma,
							Rf_gpptr(dd)->lty,
							Rf_gpptr(dd)->lwd,
							((GEDevDesc*)dd)->dev);
		    else
			Rf_dpptr(dd)->polygon(npts, xcc, ycc, DEVICE, bg, fg, dd);
		}
	    }
	    vmaxset(vmax);
	}
    }
}

/* Return a code indicating how the rectangle should be clipped.
   0 means the rectangle is totally outside the clip region
   1 means the rectangle is totally inside the clip region
   2 means the rectangle intersects the clip region */
static int clipRectCode(double x0, double y0, double x1, double y1, int coords,
		     DevDesc *dd)
{
    int result;
    /* determine clipping region */
    double xmin, xmax, ymin, ymax;
    setClipRect(&xmin, &ymin, &xmax, &ymax, DEVICE, dd);

    if (xmax < xmin) {
	double swap = xmax;
	xmax = xmin;
	xmin = swap;
    }
    if (ymax < ymin) {
	double swap = ymax;
	ymax = ymin;
	ymin = swap;
    }
    /* convert x & y to DEVICE (radius already DEVICE) */
    GConvert(&x0, &y0, coords, DEVICE, dd);
    GConvert(&x1, &y1, coords, DEVICE, dd);

    if ((x0 < xmin && x1 < xmin) || (x0 > xmax && x1 > xmax) ||
	(y0 < ymin && y1 < ymin) || (y0 > ymax && y1 > ymax))
	result = 0;
    else if ((x0 > xmin && x0 < xmax) && (x1 > xmin && x1 < xmax) &&
	     (y0 > ymin && y0 < ymax) && (y1 > ymin && y1 < ymax))
	result = 1;
    else
	result = 2;

    return result;
}

/* Draw a rectangle	*/
/* Filled with color bg and outlined with color fg  */
/* These may both be NA_INTEGER	 */
void GRect(double x0, double y0, double x1, double y1, int coords,
	   int bg, int fg, DevDesc *dd)
{
    char *vmax;
    double *xc, *yc;
    int result;
    int xpdsaved = Rf_gpptr(dd)->xpd; /* -Wall */

    if (Rf_gpptr(dd)->lty == LTY_BLANK)
	fg = NA_INTEGER; /* transparent for the border */

    /* Work in device coordinates because then it is easier to
     * work with both old and new devices.
     */
    GConvert(&x0, &y0, coords, DEVICE, dd);
    GConvert(&x1, &y1, coords, DEVICE, dd);
    result = clipRectCode(x0, y0, x1, y1, DEVICE, dd);
    switch (result) {
    case 0:  /* rectangle totally clipped; draw nothing */
	break;
    case 1:  /* rectangle totally inside;  draw all */
	/* NOTE must clip in case clipping region has been made _bigger_
	 */
	if (Rf_dpptr(dd)->canClip)
	    GClip(dd);
	if (dd->newDevStruct)
	    ((GEDevDesc*) dd)->dev->rect(x0, y0, x1, y1, fg, bg,
					 Rf_gpptr(dd)->gamma,
					 Rf_gpptr(dd)->lty, Rf_gpptr(dd)->lwd,
					 ((GEDevDesc*) dd)->dev);
	else
	    Rf_dpptr(dd)->rect(x0, y0, x1, y1, DEVICE, bg, fg, dd);
	break;
    case 2:  /* rectangle intersects clip region;  use polygon clipping */
	/* If the rect is still within the device, and the device can
	 * clip, then we can still draw a rectangle.
	 */
	Rf_gpptr(dd)->xpd = 2;
	result = clipRectCode(x0, y0, x1, y1, DEVICE, dd);
	Rf_gpptr(dd)->xpd = xpdsaved;
	if (Rf_dpptr(dd)->canClip && result == 1) {
	    GClip(dd);
	    if (dd->newDevStruct)
		((GEDevDesc*) dd)->dev->rect(x0, y0, x1, y1, fg, bg,
					     Rf_gpptr(dd)->gamma,
					     Rf_gpptr(dd)->lty, Rf_gpptr(dd)->lwd,
					     ((GEDevDesc*) dd)->dev);
	    else
		Rf_dpptr(dd)->rect(x0, y0, x1, y1, DEVICE, bg, fg, dd);
	}
	/* Otherwise fake the rectangle with a polyline or polygon.
	 */
	else {
	    vmax = vmaxget();
	    xc = (double*)R_alloc(5, sizeof(double));
	    yc = (double*)R_alloc(5, sizeof(double));
	    xc[0] = x0; yc[0] = y0;
	    xc[1] = x0; yc[1] = y1;
	    xc[2] = x1; yc[2] = y1;
	    xc[3] = x1; yc[3] = y0;
	    xc[4] = x0; yc[4] = y0;
	    if (bg == NA_INTEGER) {
		Rf_gpptr(dd)->col = fg;
		GPolyline(5, xc, yc, DEVICE, dd);
	    }
	    else { /* filled rectangle */
		int npts;
		double *xcc, *ycc;
		xcc = ycc = 0;		/* -Wall */
		npts = GClipPolygon(xc, yc, 4, DEVICE, 0, xcc, ycc, dd);
		if (npts > 1) {
		    xcc = (double*)R_alloc(npts, sizeof(double));
		    ycc = (double*)R_alloc(npts, sizeof(double));
		    npts = GClipPolygon(xc, yc, 4, DEVICE,
					1, xcc, ycc, dd);
		    if (dd->newDevStruct)
			((GEDevDesc*) dd)->dev->polygon(npts, xcc, ycc,
							fg, bg,
							Rf_gpptr(dd)->gamma,
							Rf_gpptr(dd)->lty,
							Rf_gpptr(dd)->lwd,
							((GEDevDesc*)dd)->dev);
		    else
			Rf_dpptr(dd)->polygon(npts, xcc, ycc, DEVICE, bg, fg, dd);
		}
	    }
	    vmaxset(vmax);
	}
    }
}

/* Compute string width. */
double GStrWidth(char *str, GUnit units, DevDesc *dd)
{
    double w;
    static char *sbuf = NULL;

    if (dd->newDevStruct) {
	/*
	 * FIXME:  If/When GStrWidth gets passed fontfamily and lineheight
	 * use these instead of "" and 1 below
	 */
	w = GEStrWidth(str, "", Rf_gpptr(dd)->font, 1, Rf_gpptr(dd)->cex,
		       (double) Rf_gpptr(dd)->ps,
		       (GEDevDesc*) dd);
	if (units != DEVICE)
	    w = GConvertXUnits(w, DEVICE, units, dd);
    } else {
	if (sbuf) {
	    free(sbuf);
	    sbuf = NULL;
	    warning("freeing previous text buffer in GStrWidth");
	}
	w = 0;
	if(str && *str) {
	    char *s, *sb;
	    double wdash;
	    sbuf = (char*)malloc(strlen(str) + 1);
	    if (sbuf == NULL)
		error("unable to allocate memory (in GStrWidth)");
	    sb = sbuf;
	    for(s = str; ; s++) {
		if (*s == '\n' || *s == '\0') {
		    *sb = '\0';
		    wdash = Rf_dpptr(dd)->strWidth(sbuf, dd);
		    if (wdash > w) w = wdash;
		    sb = sbuf;
		}
		else *sb++ = *s;
		if (!*s) break;
	    }
	    if (units != DEVICE)
		w = GConvertXUnits(w, DEVICE, units, dd);
	}
	if (sbuf) {
	    free(sbuf);
	    sbuf = NULL;
	}
    }
    return w;
}


/* Compute string height. */

double GStrHeight(char *str, GUnit units, DevDesc *dd)
{
    double h;
    char *s;
    double asc, dsc, wid;
    int n;
    /* Count the lines of text minus one */
    n = 0;
    for(s = str; *s ; s++)
	if (*s == '\n')
	    n++;
    h = n * GConvertYUnits(1, CHARS, DEVICE, dd);
    /* Add in the ascent of the font, if available */
    GMetricInfo('M', &asc, &dsc, &wid, DEVICE, dd);
    if ((asc == 0.0) && (dsc == 0.0) && (wid == 0.0))
	asc = GConvertYUnits(1, CHARS, DEVICE, dd);
    h += asc;
    if (units != DEVICE)
	h = GConvertYUnits(h, DEVICE, units, dd);

    return h;
}

/* Return a code indicating how the text should be clipped
   NOTE that x, y indicate the bottom-left of the text
   NOTE also that x and y are in INCHES
   NOTE also also that this is a bit crude because it actually uses
   a bounding box for the entire text to determine the clipping code.
   This will mean that in certain (very rare ?) cases, a piece of
   text will be characterised as intersecting with the clipping region
   when in fact it lies totally outside the clipping region.  But
   this is not a problem because the final output will still be correct.
   0 means totally outside clip region
   1 means totally inside clip region
   2 means intersects clip region */
static int clipTextCode(double x, double y, char *str,
		     double rot, double hadj, DevDesc *dd)
{
    double x0, x1, x2, x3, y0, y1, y2, y3, left, right, bottom, top;
    double angle = DEG2RAD * rot;
    double theta1 = M_PI/2 - angle;
    double width = GStrWidth(str, INCHES, dd);
    double height = GStrHeight(str, INCHES, dd);
#ifdef HAVE_HYPOT
    double length = hypot(width, height);
#else
    double length = pythag(width, height);
#endif
    double theta2 = angle + atan2(height, width);
    x -= hadj*width*cos(angle);
    y -= hadj*width*sin(angle);
    x0 = x + height*cos(theta1);
    x1 = x;
    x2 = x + length*cos(theta2);
    x3 = x + width*cos(angle);
    y0 = y + height*sin(theta1);
    y1 = y;
    y2 = y + length*sin(theta2);
    y3 = y + width*sin(angle);
    left = fmin2(fmin2(x0, x1), fmin2(x2, x3));
    right = fmax2(fmax2(x0, x1), fmax2(x2, x3));
    bottom = fmin2(fmin2(y0, y1), fmin2(y2, y3));
    top = fmax2(fmax2(y0, y1), fmax2(y2, y3));
    return clipRectCode(left, bottom, right, top, INCHES, dd);
}

static void clipText(double x, double y, char *str, double rot,
		     int clipToDevice, double hadj, DevDesc *dd)
{
    int result = clipTextCode(x, y, str, rot, hadj, dd);
    int xpdsaved = 0; /* -Wall */
    if (clipToDevice) {
	xpdsaved = Rf_gpptr(dd)->xpd;
	Rf_gpptr(dd)->xpd = 2;
    }
    switch (result) {
    case 0:  /* text totally clipped; draw nothing */
	break;
    case 1:  /* text totally inside;  draw all */
	if (dd->newDevStruct) {
	    GConvert(&x, &y, INCHES, DEVICE, dd);
	    ((GEDevDesc*) dd)->dev->text(x, y, str, rot, hadj,
					 Rf_gpptr(dd)->col,
					 Rf_gpptr(dd)->gamma,
					 Rf_gpptr(dd)->font,
					 Rf_gpptr(dd)->cex,
					 (double) Rf_gpptr(dd)->ps,
					 ((GEDevDesc*) dd)->dev);
	} else
	    Rf_dpptr(dd)->text(x, y, INCHES, str, rot, hadj, dd);
	break;
    case 2:  /* text intersects clip region
		act according to value of clipToDevice */
	if (clipToDevice) /* Device will do clipping */
	    if (dd->newDevStruct) {
		GConvert(&x, &y, INCHES, DEVICE, dd);
		((GEDevDesc*) dd)->dev->text(x, y, str, rot, hadj,
					     Rf_gpptr(dd)->col,
					     Rf_gpptr(dd)->gamma,
					     Rf_gpptr(dd)->font,
					     Rf_gpptr(dd)->cex,
					     (double) Rf_gpptr(dd)->ps,
					     ((GEDevDesc*) dd)->dev);
	    } else
		Rf_dpptr(dd)->text(x, y, INCHES, str, rot, hadj, dd);
	else /* don't draw anything; this could be made less crude :) */
	    ;
    }
    if (clipToDevice)
	Rf_gpptr(dd)->xpd = xpdsaved;
}

/* Draw text in a plot. */
/* If you want EXACT centering of text (e.g., like in GSymbol) */
/* then pass NA_REAL for xc and yc */
void GText(double x, double y, int coords, char *str,
	   double xc, double yc, double rot, DevDesc *dd)
{
    /* Deallocate any prior string buffer */
    static char *sbuf = NULL;

    if (sbuf) {
	free(sbuf);
	sbuf = NULL;
	warning("freeing previous text buffer in GText");
    }
    if(str && *str) {
	char *s, *sb;
	int i, n;
	double xoff, yoff, hadj;
	double sin_rot, cos_rot;/* sin() & cos() of rot{ation} in radians */
	double xleft, ybottom;
	/* We work in INCHES */
	GConvert(&x, &y, coords, INCHES, dd);
	/* Count the lines of text */
	n = 1;
	for(s = str; *s ; s++)
	    if (*s == '\n')
		n += 1;
	/* Allocate a temporary buffer */
	sbuf = (char*)malloc(strlen(str) + 1);
	sb = sbuf;
	i = 0;
	sin_rot = DEG2RAD * rot;
	cos_rot = cos(sin_rot);
	sin_rot = sin(sin_rot);
	for(s = str; ; s++) {
	    if (*s == '\n' || *s == '\0') {
		*sb = '\0';
		if (n > 1) {
		    /* first determine location of THIS line */
		    if (!R_FINITE(xc))
			xc = 0.5;
		    if (!R_FINITE(yc))
			yc = 0.5;
		    yoff = (1 - yc)*(n - 1) - i;
		    yoff = GConvertYUnits(yoff, CHARS, INCHES, dd);
		    xoff = - yoff*sin_rot;
		    yoff = yoff*cos_rot;
		    xoff = x + xoff;
		    yoff = y + yoff;
		} else {
		    xoff = x;
		    yoff = y;
		}
		/* now determine bottom-left for THIS line */
		if(xc != 0.0 || yc != 0) {
		    double width, height;
		    width = GStrWidth(sbuf, INCHES, dd);
		    if (!R_FINITE(xc))
			xc = 0.5;
		    if (!R_FINITE(yc)) {
				/* "exact" vertical centering */
				/* If font metric info is available AND */
				/* there is only one line, use GMetricInfo & yc=0.5 */
				/* Otherwise use GStrHeight and fiddle yc */
			double h, d, w;
			GMetricInfo(0, &h, &d, &w, INCHES, dd);
			if (n>1 || (h==0 && d==0 && w==0)) {
			    height = GStrHeight(sbuf, INCHES, dd);
			    yc = Rf_dpptr(dd)->yCharOffset;
			} else {
			    double maxHeight = 0.0;
			    double maxDepth = 0.0;
			    char *ss;
			    int charNum = 0;
			    for (ss=sbuf; *ss; ss++) {
				GMetricInfo((unsigned char) *ss, &h, &d, &w,
					    INCHES, dd);
				/* Set maxHeight and maxDepth from height
				   and depth of first char.
				   Must NOT set to 0 in case there is
				   only 1 char and it has negative
				   height or depth
				*/
				if (charNum++ == 0) {
				    maxHeight = h;
				    maxDepth = d;
				} else {
				    if (h > maxHeight) maxHeight = h;
				    if (d > maxDepth) maxDepth = d;
				}
			    }
			    height = maxHeight - maxDepth;
			    yc = 0.5;
			}
		    } else {
			height = GStrHeight(sbuf, INCHES, dd);
		    }
		    if (Rf_dpptr(dd)->canHAdj == 2) hadj = xc;
		    else if (Rf_dpptr(dd)->canHAdj == 1) {
			hadj = 0.5 * floor(2*xc + 0.5);
				/* limit to 0, 0.5, 1 */
			hadj = (hadj > 1.0) ? 1.0 :((hadj < 0.0) ? 0.0 : hadj);
		    } else hadj = 0.0;
		    xleft = xoff - (xc-hadj)*width*cos_rot + yc*height*sin_rot;
		    ybottom= yoff - (xc-hadj)*width*sin_rot - yc*height*cos_rot;
		} else { /* xc = yc = 0.0 */
		    xleft = xoff;
		    ybottom = yoff;
		    hadj = 0.0;
		}
		if(Rf_dpptr(dd)->canClip) {
		    GClip(dd);
		    clipText(xleft, ybottom, sbuf, rot, 1, hadj, dd);
		} else
		    clipText(xleft, ybottom, sbuf, rot, 0, hadj, dd);
		sb = sbuf;
		i += 1;
	    }
	    else *sb++ = *s;
	    if (!*s) break;
	}
	if (sbuf) {
	    free(sbuf);
	    sbuf = NULL;
	}
    }
}

/*-------------------------------------------------------------------
 *
 *  GRAPHICAL UTILITIES
 *
 */


/* GArrow -- Draw an arrow. */
/* NOTE that the length parameter is in inches. */
void GArrow(double xfrom, double yfrom, double xto, double yto, int coords,
	    double length, double angle, int code, DevDesc *dd)
{

    double xfromInch = xfrom;
    double yfromInch = yfrom;
    double xtoInch = xto;
    double ytoInch = yto;
    double rot, xc, yc;
    double x[3], y[3];
    double eps = 1.e-3;

    GLine(xfrom, yfrom, xto, yto, coords, dd);

    GConvert(&xfromInch, &yfromInch, coords, INCHES, dd);
    GConvert(&xtoInch, &ytoInch, coords, INCHES, dd);
    if((code & 3) == 0) return; /* no arrows specified */
    if(length == 0) return; /* zero-length arrow heads */

#ifdef HAVE_HYPOT
    if(hypot(xfromInch - xtoInch, yfromInch - ytoInch) < eps) {
#else
    if(pythag(xfromInch - xtoInch, yfromInch - ytoInch) < eps) {
#endif
	/* effectively 0-length arrow */
	warning("zero-length arrow is of indeterminate angle and so skipped");
	return;
    }
    angle *= DEG2RAD;
    if(code & 1) {
	xc = xtoInch - xfromInch;
	yc = ytoInch - yfromInch;
	rot= atan2(yc, xc);
	x[0] = xfromInch + length * cos(rot+angle);
	y[0] = yfromInch + length * sin(rot+angle);
	x[1] = xfromInch;
	y[1] = yfromInch;
	x[2] = xfromInch + length * cos(rot-angle);
	y[2] = yfromInch + length * sin(rot-angle);
	GPolyline(3, x, y, INCHES, dd);
    }
    if(code & 2) {
	xc = xfromInch - xtoInch;
	yc = yfromInch - ytoInch;
	rot= atan2(yc, xc);
	x[0] = xtoInch + length * cos(rot+angle);
	y[0] = ytoInch + length * sin(rot+angle);
	x[1] = xtoInch;
	y[1] = ytoInch;
	x[2] = xtoInch + length * cos(rot-angle);
	y[2] = ytoInch + length * sin(rot-angle);
	GPolyline(3, x, y, INCHES, dd);
    }
}


/* Draw a box about one of several regions:  box(which) */
void GBox(int which, DevDesc *dd)
{
    double x[7], y[7];
    if (which == 1) {/* plot */
	x[0] = Rf_gpptr(dd)->plt[0]; y[0] = Rf_gpptr(dd)->plt[2];/* <- , __ */
	x[1] = Rf_gpptr(dd)->plt[1]; y[1] = Rf_gpptr(dd)->plt[2];/* -> , __ */
	x[2] = Rf_gpptr(dd)->plt[1]; y[2] = Rf_gpptr(dd)->plt[3];/* -> , ^  */
	x[3] = Rf_gpptr(dd)->plt[0]; y[3] = Rf_gpptr(dd)->plt[3];/* <- , ^  */
	x[4] = x[0];	      y[4] = y[0];	   /* <- , __ */
	x[5] = x[1];	      y[5] = y[1];	   /* -> , __ */
	x[6] = x[2];	      y[6] = y[2];	   /* -> , __ */
    }
    else {/* "figure", "inner", or "outer" */
	x[0] = 0.; y[0] = 0.;
	x[1] = 1.; y[1] = 0.;
	x[2] = 1.; y[2] = 1.;
	x[3] = 0.; y[3] = 1.;
    }
    switch(which) {
    case 1: /* Plot */
	switch(Rf_gpptr(dd)->bty) {
	case 'o':
	case 'O':
	    GPolygon(4, x, y, NFC, NA_INTEGER, Rf_gpptr(dd)->col, dd);
	    break;
	case 'l':
	case 'L':
	    GPolyline(3, x+3, y+3, NFC, dd);
	    break;
	case '7':
	    GPolyline(3, x+1, y+1, NFC, dd);
	    break;
	case 'c':
	case 'C':
	case '[':
	    GPolyline(4, x+2, y+2, NFC, dd);
	    break;
	case ']':/* new */
	    GPolyline(4, x, y, NFC, dd);
	    break;
	case 'u':
	case 'U':
	    GPolyline(4, x+3, y+3, NFC, dd);
	    break;
	case 'n':
	case 'N': /* nothing */
	    break;
	default:
	    warning("invalid par(\"bty\") = '%c'; no box() drawn.",
		    Rf_gpptr(dd)->bty);
	}
	break;
    case 2: /* Figure */
	GPolygon(4, x, y, NFC, NA_INTEGER, Rf_gpptr(dd)->col, dd);
	break;
    case 3: /* Inner Region */
	GPolygon(4, x, y, NIC, NA_INTEGER, Rf_gpptr(dd)->col, dd);
	break;
    case 4: /* "outer": Device border */
	GPolygon(4, x, y, NDC, NA_INTEGER, Rf_gpptr(dd)->col, dd);
	break;
    default:
	error("invalid GBox argument");
    }
}


#define LPR_SMALL  2
#define LPR_MEDIUM 3

void GLPretty(double *ul, double *uh, int *n)
{
/* Generate pretty tick values --	LOGARITHMIC scale
 * __ ul < uh __
 * This only does a very simple setup.
 * The real work happens when the axis is drawn. */
    int p1, p2;
    p1 = ceil(log10(*ul));
    p2 = floor(log10(*uh));

    if (p2 - p1 <= 0) { /* floor(log10(uh)) <= ceil(log10(ul))
			 * <==>	 log10(uh) - log10(ul) < 2
			 * <==>		uh / ul	       < 100 */
	/* Very small range : Use tickmarks from a LINEAR scale
	 *		      Splus uses n = 9 here, but that is dumb */
	GPretty(ul, uh, n);
	*n = -*n;
    }
    else { /* extra tickmarks --> CreateAtVector(.) in	../plot.c */
	*ul = pow(10., (double)p1);
	*uh = pow(10., (double)p2);
	if (p2 - p1 <= LPR_SMALL)
	    *n = 3; /* Small range :	Use 1,2,5,10 times 10^k tickmarks */
	else if (p2 - p1 <= LPR_MEDIUM)
	    *n = 2; /* Medium range :	Use 1,5 times 10^k tickmarks */
	else
	    *n = 1; /* Large range :	Use 10^k tickmarks
		     *			But decimate, when there are too many*/
    }
}

void GPretty(double *lo, double *up, int *ndiv)
{
/*	Set scale and ticks for linear scales.
 *	Called from GScale() and GSetupAxis().
 *
 *	Pre:	    x1 == lo < up == x2      ;  ndiv >= 1
 *	Post: x1 <= y1 := lo < up =: y2 <= x2;	ndiv >= 1
 */
    double unit, ns, nu;
    double high_u_fact[2] = { .8, 1.7 };
#ifdef DEBUG_PLOT
    double x1,x2;
#endif

    if(*ndiv <= 0)
	error("invalid axis extents [GPretty(.,.,n=%d)", *ndiv);
    if(*lo == R_PosInf || *up == R_PosInf ||
       *lo == R_NegInf || *up == R_NegInf ||
       !R_FINITE(*up - *lo)) {
	error("Infinite axis extents [GPretty(%g,%g,%d)]", *lo, *up, *ndiv);
	return;/*-Wall*/
    }

    ns = *lo; nu = *up;
#ifdef DEBUG_PLOT
    x1 = ns; x2 = nu;
#endif
    unit = R_pretty0(&ns, &nu, ndiv, /* min_n = */ 1,
		     /* shrink_sml = */ 0.25,
		     high_u_fact,
		     2, /* do eps_correction in any case */
		     0 /* return (ns,nu) in  (lo,up) */);
    /* ==> ../appl/pretty.c */

    /* The following is ugly since it kind of happens already in Rpretty0(..):
     */
#define rounding_eps 1e-7
    if(nu >= ns + 1) {
	if(               ns * unit < *lo - rounding_eps*unit)
	    ns++;
	if(nu > ns + 1 && nu * unit > *up + rounding_eps*unit)
	    nu--;
	*ndiv = nu - ns;
    }
    *lo = ns * unit;
    *up = nu * unit;
#ifdef non_working_ALTERNATIVE
    if(ns * unit > *lo)
	*lo = ns * unit;
    if(nu * unit < *up)
	*up = nu * unit;
    if(nu - ns >= 1)
	*ndiv = nu - ns;
#endif

#ifdef DEBUG_PLOT
    if(*lo < x1)
	warning(" .. GPretty(.): new *lo = %g < %g = x1", *lo, x1);
    if(*up > x2)
	warning(" .. GPretty(.): new *up = %g > %g = x2", *up, x2);
#endif
}


#define SMALL	0.25
#define RADIUS	0.375
#define SQRC	0.88622692545275801364		/* sqrt(pi / 4) */
#define DMDC	1.25331413731550025119		/* sqrt(pi / 4) * sqrt(2) */
#define TRC0	1.55512030155621416073		/* sqrt(4 * pi/(3 * sqrt(3))) */
#define TRC1	1.34677368708859836060		/* TRC0 * sqrt(3) / 2 */
#define TRC2	0.77756015077810708036		/* TRC0 / 2 */
#define CMAG	1.0				/* Circle magnifier, now defunct */
#define GSTR_0  Rf_dpptr(dd)->cra[1] * 0.5 * Rf_gpptr(dd)->ipr[0] * Rf_gpptr(dd)->cex
/* NOTE: This cex is already multiplied with cexbase */

/* Draw one of the R special symbols. */
void GSymbol(double x, double y, int coords, int pch, DevDesc *dd)
{
    double r, xc, yc;
    double xx[4], yy[4];
    char str[2];
    int ltyOld;

    if(' ' <= pch && pch <= 255) {
	if (pch == '.') {
	    GConvert(&x, &y, coords, DEVICE, dd);
	    GRect(x-.5, y-.5, x+.5, y+.5, DEVICE, Rf_gpptr(dd)->col, NA_INTEGER, dd);
	} else {
	    str[0] = pch;
	    str[1] = '\0';
	    GText(x, y, coords, str, NA_REAL, NA_REAL, 0., dd);
	}
    }
    else {
	ltyOld = Rf_gpptr(dd)->lty;
	Rf_gpptr(dd)->lty = LTY_SOLID;

	switch(pch) {

	case 0: /* S square */
	    xc = RADIUS * GSTR_0;
	    GConvert(&x, &y, coords, INCHES, dd);
	    GRect(x-xc, y-xc, x+xc, y+xc, INCHES, NA_INTEGER,
		  Rf_gpptr(dd)->col, dd);
	    break;

	case 1: /* S octahedron ( circle) */
	    xc = CMAG * RADIUS * GSTR_0;
	    GCircle(x, y, coords, xc, NA_INTEGER, Rf_gpptr(dd)->col, dd);
	    break;

	case 2:	/* S triangle - point up */
	    xc = RADIUS * GSTR_0;
	    GConvert(&x, &y, coords, INCHES, dd);
	    r = TRC0 * xc;
	    yc = TRC2 * xc;
	    xc = TRC1 * xc;
	    xx[0] = x; yy[0] = y+r;
	    xx[1] = x+xc; yy[1] = y-yc;
	    xx[2] = x-xc; yy[2] = y-yc;
	    GPolygon(3, xx, yy, INCHES, NA_INTEGER, Rf_gpptr(dd)->col, dd);
	    break;

	case 3: /* S plus */
	    xc = M_SQRT2*RADIUS*GSTR_0;
	    GConvert(&x, &y, coords, INCHES, dd);
	    GLine(x-xc, y, x+xc, y, INCHES, dd);
	    GLine(x, y-xc, x, y+xc, INCHES, dd);
	    break;

	case 4: /* S times */
	    xc = RADIUS * GSTR_0;
	    GConvert(&x, &y, coords, INCHES, dd);
	    GLine(x-xc, y-xc, x+xc, y+xc, INCHES, dd);
	    GLine(x-xc, y+xc, x+xc, y-xc, INCHES, dd);
	    break;

	case 5: /* S diamond */
	    xc = M_SQRT2 * RADIUS * GSTR_0;
	    GConvert(&x, &y, coords, INCHES, dd);
	    xx[0] = x-xc; yy[0] = y;
	    xx[1] = x; yy[1] = y+xc;
	    xx[2] = x+xc; yy[2] = y;
	    xx[3] = x; yy[3] = y-xc;
	    GPolygon(4, xx, yy, INCHES, NA_INTEGER, Rf_gpptr(dd)->col, dd);
	    break;

	case 6: /* S triangle - point down */
	    xc = RADIUS * GSTR_0;
	    GConvert(&x, &y, coords, INCHES, dd);
	    r = TRC0 * xc;
	    yc = TRC2 * xc;
	    xc = TRC1 * xc;
	    xx[0] = x; yy[0] = y-r;
	    xx[1] = x+xc; yy[1] = y+yc;
	    xx[2] = x-xc; yy[2] = y+yc;
	    GPolygon(3, xx, yy, INCHES, NA_INTEGER, Rf_gpptr(dd)->col, dd);
	    break;

	case 7:	/* S square and times superimposed */
	    xc =  RADIUS * GSTR_0;
	    GConvert(&x, &y, coords, INCHES, dd);
	    GLine(x-xc, y-xc, x+xc, y+xc, INCHES, dd);
	    GLine(x-xc, y+xc, x+xc, y-xc, INCHES, dd);
	    GRect(x-xc, y-xc, x+xc, y+xc, INCHES, NA_INTEGER,
		  Rf_gpptr(dd)->col, dd);
	    break;

	case 8: /* S plus and times superimposed */
	    xc =  RADIUS * GSTR_0;
	    GConvert(&x, &y, coords, INCHES, dd);
	    GLine(x-xc, y-xc, x+xc, y+xc, INCHES, dd);
	    GLine(x-xc, y+xc, x+xc, y-xc, INCHES, dd);
	    xc = M_SQRT2 * xc;
	    GLine(x-xc, y, x+xc, y, INCHES, dd);
	    GLine(x, y-xc, x, y+xc, INCHES, dd);
	    break;

	case 9: /* S diamond and plus superimposed */
	    xc = M_SQRT2 * RADIUS * GSTR_0;
	    GConvert(&x, &y, coords, INCHES, dd);
	    xx[0] = x-xc; yy[0] = y;
	    xx[1] = x; yy[1] = y+xc;
	    xx[2] = x+xc; yy[2] = y;
	    xx[3] = x; yy[3] = y-xc;
	    GPolygon(4, xx, yy, INCHES, NA_INTEGER, Rf_gpptr(dd)->col, dd);
	    GLine(x-xc, y, x+xc, y, INCHES, dd);
	    GLine(x, y-xc, x, y+xc, INCHES, dd);
	    break;

	case 10: /* S hexagon (circle) and plus superimposed */
	    xc = CMAG * RADIUS * GSTR_0;
	    GCircle(x, y, coords, xc, NA_INTEGER, Rf_gpptr(dd)->col, dd);
	    GConvert(&x, &y, coords, INCHES, dd);
	    GLine(x-xc, y, x+xc, y, INCHES, dd);
	    GLine(x, y-xc, x, y+xc, INCHES, dd);
	    break;

	case 11: /* S superimposed triangles */
	    xc = RADIUS * GSTR_0;
	    GConvert(&x, &y, coords, INCHES, dd);
	    r = TRC0 * xc;
	    yc = TRC2 * xc;
	    yc = 0.5 * (yc + r);
	    xc = TRC1 * xc;
	    xx[0] = x; yy[0] = y+r;
	    xx[1] = x+xc; yy[1] = y-yc;
	    xx[2] = x-xc; yy[2] = y-yc;
	    GPolygon(3, xx, yy, INCHES, NA_INTEGER, Rf_gpptr(dd)->col, dd);
	    xx[0] = x; yy[0] = y-r;
	    xx[1] = x+xc; yy[1] = y+yc;
	    xx[2] = x-xc; yy[2] = y+yc;
	    GPolygon(3, xx, yy, INCHES, NA_INTEGER, Rf_gpptr(dd)->col, dd);
	    break;

	case 12: /* S square and plus superimposed */
	    xc = RADIUS * GSTR_0;
	    GConvert(&x, &y, coords, INCHES, dd);
	    GLine(x-xc, y, x+xc, y, INCHES, dd);
	    GLine(x, y-xc, x, y+xc, INCHES, dd);
	    GRect(x-xc, y-xc, x+xc, y+xc, INCHES,
		  NA_INTEGER, Rf_gpptr(dd)->col, dd);
	    break;

	case 13: /* S octagon (circle) and times superimposed */
	    xc = CMAG * RADIUS * GSTR_0;
	    GCircle(x, y, coords, xc, NA_INTEGER, Rf_gpptr(dd)->col, dd);
	    GConvert(&x, &y, coords, INCHES, dd);
	    GLine(x-xc, y-xc, x+xc, y+xc, INCHES, dd);
	    GLine(x-xc, y+xc, x+xc, y-xc, INCHES, dd);
	    break;

	case 14: /* S square and point-up triangle superimposed */
	    xc = RADIUS * GSTR_0;
	    GConvert(&x, &y, coords, INCHES, dd);
	    xx[0] = x; yy[0] = y+xc;
	    xx[1] = x+xc; yy[1] = y-xc;
	    xx[2] = x-xc; yy[2] = y-xc;
	    GPolygon(3, xx, yy, INCHES, NA_INTEGER, Rf_gpptr(dd)->col, dd);
	    GRect(x-xc, y-xc, x+xc, y+xc, INCHES,
		  NA_INTEGER, Rf_gpptr(dd)->col, dd);
	    break;

	case 15: /* S filled square */
	    xc = RADIUS * GSTR_0;
	    GConvert(&x, &y, coords, INCHES, dd);
	    xx[0] = x-xc; yy[0] = y-xc;
	    xx[1] = x+xc; yy[1] = y-xc;
	    xx[2] = x+xc; yy[2] = y+xc;
	    xx[3] = x-xc; yy[3] = y+xc;
	    GPolygon(4, xx, yy, INCHES, Rf_gpptr(dd)->col,
		     NA_INTEGER, dd);
	    break;

	case 16: /* S filled octagon (circle) */
	    xc = RADIUS * GSTR_0;
	    GCircle(x, y, coords, xc, Rf_gpptr(dd)->col, Rf_gpptr(dd)->col, dd);
	    break;

	case 17: /* S filled point-up triangle */
	    xc = RADIUS * GSTR_0;
	    GConvert(&x, &y, coords, INCHES, dd);
	    r = TRC0 * xc;
	    yc = TRC2 * xc;
	    xc = TRC1 * xc;
	    xx[0] = x;	  yy[0] = y+r;
	    xx[1] = x+xc; yy[1] = y-yc;
	    xx[2] = x-xc; yy[2] = y-yc;
	    GPolygon(3, xx, yy, INCHES, Rf_gpptr(dd)->col,
		     NA_INTEGER, dd);
	    break;

	case 18: /* S filled diamond */
	    xc = RADIUS * GSTR_0;
	    GConvert(&x, &y, coords, INCHES, dd);
	    xx[0] = x;	  yy[0] = y-xc;
	    xx[1] = x+xc; yy[1] = y;
	    xx[2] = x;	  yy[2] = y+xc;
	    xx[3] = x-xc; yy[3] = y;
	    GPolygon(4, xx, yy, INCHES, Rf_gpptr(dd)->col,
		     NA_INTEGER, dd);
	    break;

	case 19: /* R filled circle */
	    xc = RADIUS * GSTR_0;
	    GCircle(x, y, coords, xc, Rf_gpptr(dd)->col, Rf_gpptr(dd)->col, dd);
	    break;


	case 20: /* R `Dot' (small circle) */
	    xc = SMALL * GSTR_0;
	    GCircle(x, y, coords, xc, Rf_gpptr(dd)->col, Rf_gpptr(dd)->col, dd);
	    break;


	case 21: /* circles */
	    xc = RADIUS * CMAG * GSTR_0;
	    GCircle(x, y, coords, xc, Rf_gpptr(dd)->bg, Rf_gpptr(dd)->col, dd);
	    break;

	case  22: /* squares */
	    xc = RADIUS * SQRC * GSTR_0;
	    GConvert(&x, &y, coords, INCHES, dd);
	    GRect(x-xc, y-xc, x+xc, y+xc, INCHES,
		  Rf_gpptr(dd)->bg, Rf_gpptr(dd)->col, dd);
	    break;

	case 23: /* diamonds */
	    xc = RADIUS * DMDC * GSTR_0;
	    GConvert(&x, &y, coords, INCHES, dd);
	    xx[0] = x	  ; yy[0] = y-xc;
	    xx[1] = x+xc; yy[1] = y;
	    xx[2] = x	  ; yy[2] = y+xc;
	    xx[3] = x-xc; yy[3] = y;
	    GPolygon(4, xx, yy, INCHES,
		     Rf_gpptr(dd)->bg, Rf_gpptr(dd)->col, dd);
	    break;

	case 24: /* triangle (point up) */
	    xc = RADIUS * GSTR_0;
	    GConvert(&x, &y, coords, INCHES, dd);
	    r = TRC0 * xc;
	    yc = TRC2 * xc;
	    xc = TRC1 * xc;
	    xx[0] = x;	  yy[0] = y+r;
	    xx[1] = x+xc; yy[1] = y-yc;
	    xx[2] = x-xc; yy[2] = y-yc;
	    GPolygon(3, xx, yy, INCHES,
		     Rf_gpptr(dd)->bg, Rf_gpptr(dd)->col, dd);
	    break;

	case 25: /* triangle (point down) */
	    xc = RADIUS * GSTR_0;
	    GConvert(&x, &y, coords, INCHES, dd);
	    r = TRC0 * xc;
	    yc = TRC2 * xc;
	    xc = TRC1 * xc;
	    xx[0] = x;	  yy[0] = y-r;
	    xx[1] = x+xc; yy[1] = y+yc;
	    xx[2] = x-xc; yy[2] = y+yc;
	    GPolygon(3, xx, yy, INCHES,
		     Rf_gpptr(dd)->bg, Rf_gpptr(dd)->col, dd);
	    break;
	}
	Rf_gpptr(dd)->lty = ltyOld;
    }
}


/* Draw text in plot margins. */
void GMtext(char *str, int side, double line, int outer, double at, int las,
	    DevDesc *dd)
{
/* "las" gives the style of axis labels:
	 0 = always parallel to the axis [= default],
	 1 = always horizontal,
	 2 = always perpendicular to the axis.
	 3 = always vertical.
*/
    double angle, xadj, yadj;
    int coords, subcoords;

    /* Init to keep -Wall happy: */
    angle = 0.;
    coords = 0;

    xadj = Rf_gpptr(dd)->adj;	/* ALL cases */
    yadj = 0.;		/* Default; currently all cases */
    if(outer) {
	switch(side) {
	case 1:	    coords = OMA1;	break;
	case 2:	    coords = OMA2;	break;
	case 3:	    coords = OMA3;	break;
	case 4:	    coords = OMA4;	break;
	}
	subcoords = NIC;
    }
    else {
	switch(side) {
	case 1:	    coords = MAR1;	break;
	case 2:	    coords = MAR2;	break;
	case 3:	    coords = MAR3;	break;
	case 4:	    coords = MAR4;	break;
	}
	subcoords = USER;
    }
    /* Note: I changed Rf_gpptr(dd)->yLineBias to 0.3 here. */
    /* Purely visual tuning. RI */
    switch(side) {
    case 1:
	if(las == 2 || las == 3) {
	    at = GConvertX(at, subcoords, LINES, dd) + 0.3;
	    at = GConvertX(at, LINES, subcoords, dd);
	    angle = 90;
	}
	else {
	    line = line + 1 - Rf_gpptr(dd)->yLineBias;
	    angle = 0;
	}
	break;
    case 2:
	if(las == 1 || las == 2) {
	    /* subcoords could be USER and the user could have set log="y"
	     * If that's the case then converting a height to USER
	     * coordinates will not work
	     * SO to be safe, we convert "at" to a LINES location,
	     * add the 0.3 and then convert the result back to a USER
	     * lcoation (ok because converting _locations_ is ok)
	     * The old, bad way to do it was:
	     *     at = at - GConvertYUnits(0.3, LINES, subcoords, dd);
	     */
	    at = GConvertY(at, subcoords, LINES, dd) - 0.3;
	    at = GConvertY(at, LINES, subcoords, dd);
	    angle = 0;
	}
	else {
	    line = line + Rf_gpptr(dd)->yLineBias;
	    angle = 90;
	}
	break;
    case 3:
	if(las == 2 || las == 3) {
	    at = GConvertX(at, subcoords, LINES, dd) + 0.3;
	    at = GConvertX(at, LINES, subcoords, dd);
	    angle = 90;
	}
	else {
	    line = line + Rf_gpptr(dd)->yLineBias;
	    angle = 0;
	}
	break;
    case 4:
	if(las == 1 || las == 2) {
	    at = GConvertY(at, subcoords, LINES, dd) - 0.3;
	    at = GConvertY(at, LINES, subcoords, dd);
	    angle = 0;
	}
	else {
	    line = line + 1 - Rf_gpptr(dd)->yLineBias;
	    angle = 90;
	}
	break;
    }
    GText(at, line, coords, str, xadj, yadj, angle, dd);
}/* GMtext */


/* Colour Code */

/* hsv2rgb -- HSV to RGB conversion  */
/* Based on HSV_TO_RGB from Foley and Van Dam First Ed. Page 616 */
/* See Alvy Ray Smith, Color Gamut Transform Pairs, SIGGRAPH '78 */

void hsv2rgb(double h, double s, double v, double *r, double *g, double *b)
{
    double f, p, q, t;
    int i;

    f = modf(h * 6.0, &t);
    i = ((int) t) % 6;

    p = v * (1 - s);
    q = v * (1 - s * f);
    t = v * (1 - (s * (1 - f)));
    switch (i) {
    case 0:	*r = v;		*g = t;		*b = p;	break;
    case 1:	*r = q;		*g = v;		*b = p;	break;
    case 2:	*r = p;		*g = v;		*b = t;	break;
    case 3:	*r = p;		*g = q;		*b = v; break;
    case 4:	*r = t;		*g = p;		*b = v; break;
    case 5:	*r = v;		*g = p;		*b = q;	break;
    default:
	error("bad hsv to rgb color conversion");
    }
}

/* rgb2hsv() -- the reverse (same reference as above
 *	this implementation is adapted from code by Nicholas Lewin-Koh.
 */
void rgb2hsv(double r, double g, double b,
	     double *h, double *s, double *v)
{
    double min, max, delta;

    /* Compute  min(r,g,b) and max(r,g,b): */
    min = max = r;
    if(min > g) { /* g < r */
	if(b < g)
	    min = b;/* &  max = r */
        else { /* g <= b, g < r */
	    min = g;
	    if(b > r) max = g; /* else : g <= b <=r */
	}
    } else { /* r <= g */
	if(b > g)
	    max = b;/* &  min = r */
        else { /* b,r <= g */
	    max = g;
	    if(b < r) min = b; /* else : r <= b <= g */
	}
    }

    *v = max;
    if( max == 0 || (delta = max - min) == 0) {
	/*   r = g = b : "gray" : s = 0, h is undefined */
	*s = 0;
	*h = NA_REAL;
	return;
    }
    /* else : */
    *s = delta / max;

    if( r == max )
	*h = ( g - b ) / delta;  /* between yellow & magenta */
    else if( g == max )
	*h = 2 + ( b - r ) / delta; /* between cyan & yellow*/
    else
	*h = 4 + ( r - g ) / delta; /* between magenta & cyan */

    *h *= 60; /* degrees */
    if(*h < 0)
	*h += 360;
    *h /= 360;
    return;
}


/*
 *  Color Specification
 *
 *  Colors are stored internally in integers.  Each integer is
 *  broken into four bytes.	 The three least significant bytes
 *  are used to contain levels of red, green and blue.	These
 *  levels are integers in the range [0,255].
 *
 *  Externally, colors are specified either:
 *
 *    a) by name, using a large table of color names,
 *
 *    b) by RGB values using a string of the form "#rrggbb"
 *	 where rr, gg and bb are hex integers giving the level
 *	 of red green and blue,
 *
 *    c) as an index into a user setable palette of colors.
 *
 */

/* Default Color Palette */
/* Paul Murrell 05/06/02
 * Changed "white" to "grey" in the default palette
 * in response to user suggestion
 */

char *DefaultPalette[] = {
    "black",
    "red",
    "green3",
    "blue",
    "cyan",
    "magenta",
    "yellow",
    "grey",
    NULL
};

/* The Table of Known Color Names */
/* Adapted from the X11 RGB database */
/* Note: the color "white" is moved */
/* to the top of the database to avoid */
/* its being known as "gray100" */

static int ColorDataBaseSize;

ColorDataBaseEntry ColorDataBase[] = {
    /* name		rgb         code -- filled in by InitColors() */
    {"white",		"#FFFFFF",	0},
    {"aliceblue",	"#F0F8FF",	0},
    {"antiquewhite",	"#FAEBD7",	0},
    {"antiquewhite1",	"#FFEFDB",	0},
    {"antiquewhite2",	"#EEDFCC",	0},
    {"antiquewhite3",	"#CDC0B0",	0},
    {"antiquewhite4",	"#8B8378",	0},
    {"aquamarine",	"#7FFFD4",	0},
    {"aquamarine1",	"#7FFFD4",	0},
    {"aquamarine2",	"#76EEC6",	0},
    {"aquamarine3",	"#66CDAA",	0},
    {"aquamarine4",	"#458B74",	0},
    {"azure",		"#F0FFFF",	0},
    {"azure1",		"#F0FFFF",	0},
    {"azure2",		"#E0EEEE",	0},
    {"azure3",		"#C1CDCD",	0},
    {"azure4",		"#838B8B",	0},
    {"beige",		"#F5F5DC",	0},
    {"bisque",		"#FFE4C4",	0},
    {"bisque1",		"#FFE4C4",	0},
    {"bisque2",		"#EED5B7",	0},
    {"bisque3",		"#CDB79E",	0},
    {"bisque4",		"#8B7D6B",	0},
    {"black",		"#000000",	0},
    {"blanchedalmond",	"#FFEBCD",	0},
    {"blue",		"#0000FF",	0},
    {"blue1",		"#0000FF",	0},
    {"blue2",		"#0000EE",	0},
    {"blue3",		"#0000CD",	0},
    {"blue4",		"#00008B",	0},
    {"blueviolet",	"#8A2BE2",	0},
    {"brown",		"#A52A2A",	0},
    {"brown1",		"#FF4040",	0},
    {"brown2",		"#EE3B3B",	0},
    {"brown3",		"#CD3333",	0},
    {"brown4",		"#8B2323",	0},
    {"burlywood",	"#DEB887",	0},
    {"burlywood1",	"#FFD39B",	0},
    {"burlywood2",	"#EEC591",	0},
    {"burlywood3",	"#CDAA7D",	0},
    {"burlywood4",	"#8B7355",	0},
    {"cadetblue",	"#5F9EA0",	0},
    {"cadetblue1",	"#98F5FF",	0},
    {"cadetblue2",	"#8EE5EE",	0},
    {"cadetblue3",	"#7AC5CD",	0},
    {"cadetblue4",	"#53868B",	0},
    {"chartreuse",	"#7FFF00",	0},
    {"chartreuse1",	"#7FFF00",	0},
    {"chartreuse2",	"#76EE00",	0},
    {"chartreuse3",	"#66CD00",	0},
    {"chartreuse4",	"#458B00",	0},
    {"chocolate",	"#D2691E",	0},
    {"chocolate1",	"#FF7F24",	0},
    {"chocolate2",	"#EE7621",	0},
    {"chocolate3",	"#CD661D",	0},
    {"chocolate4",	"#8B4513",	0},
    {"coral",		"#FF7F50",	0},
    {"coral1",		"#FF7256",	0},
    {"coral2",		"#EE6A50",	0},
    {"coral3",		"#CD5B45",	0},
    {"coral4",		"#8B3E2F",	0},
    {"cornflowerblue",	"#6495ED",	0},
    {"cornsilk",	"#FFF8DC",	0},
    {"cornsilk1",	"#FFF8DC",	0},
    {"cornsilk2",	"#EEE8CD",	0},
    {"cornsilk3",	"#CDC8B1",	0},
    {"cornsilk4",	"#8B8878",	0},
    {"cyan",		"#00FFFF",	0},
    {"cyan1",		"#00FFFF",	0},
    {"cyan2",		"#00EEEE",	0},
    {"cyan3",		"#00CDCD",	0},
    {"cyan4",		"#008B8B",	0},
    {"darkblue",	"#00008B",	0},
    {"darkcyan",	"#008B8B",	0},
    {"darkgoldenrod",	"#B8860B",	0},
    {"darkgoldenrod1",	"#FFB90F",	0},
    {"darkgoldenrod2",	"#EEAD0E",	0},
    {"darkgoldenrod3",	"#CD950C",	0},
    {"darkgoldenrod4",	"#8B6508",	0},
    {"darkgray",	"#A9A9A9",	0},
    {"darkgreen",	"#006400",	0},
    {"darkgrey",	"#A9A9A9",	0},
    {"darkkhaki",	"#BDB76B",	0},
    {"darkmagenta",	"#8B008B",	0},
    {"darkolivegreen",	"#556B2F",	0},
    {"darkolivegreen1",	"#CAFF70",	0},
    {"darkolivegreen2",	"#BCEE68",	0},
    {"darkolivegreen3",	"#A2CD5A",	0},
    {"darkolivegreen4",	"#6E8B3D",	0},
    {"darkorange",	"#FF8C00",	0},
    {"darkorange1",	"#FF7F00",	0},
    {"darkorange2",	"#EE7600",	0},
    {"darkorange3",	"#CD6600",	0},
    {"darkorange4",	"#8B4500",	0},
    {"darkorchid",	"#9932CC",	0},
    {"darkorchid1",	"#BF3EFF",	0},
    {"darkorchid2",	"#B23AEE",	0},
    {"darkorchid3",	"#9A32CD",	0},
    {"darkorchid4",	"#68228B",	0},
    {"darkred",		"#8B0000",	0},
    {"darksalmon",	"#E9967A",	0},
    {"darkseagreen",	"#8FBC8F",	0},
    {"darkseagreen1",	"#C1FFC1",	0},
    {"darkseagreen2",	"#B4EEB4",	0},
    {"darkseagreen3",	"#9BCD9B",	0},
    {"darkseagreen4",	"#698B69",	0},
    {"darkslateblue",	"#483D8B",	0},
    {"darkslategray",	"#2F4F4F",	0},
    {"darkslategray1",	"#97FFFF",	0},
    {"darkslategray2",	"#8DEEEE",	0},
    {"darkslategray3",	"#79CDCD",	0},
    {"darkslategray4",	"#528B8B",	0},
    {"darkslategrey",	"#2F4F4F",	0},
    {"darkturquoise",	"#00CED1",	0},
    {"darkviolet",	"#9400D3",	0},
    {"deeppink",	"#FF1493",	0},
    {"deeppink1",	"#FF1493",	0},
    {"deeppink2",	"#EE1289",	0},
    {"deeppink3",	"#CD1076",	0},
    {"deeppink4",	"#8B0A50",	0},
    {"deepskyblue",	"#00BFFF",	0},
    {"deepskyblue1",	"#00BFFF",	0},
    {"deepskyblue2",	"#00B2EE",	0},
    {"deepskyblue3",	"#009ACD",	0},
    {"deepskyblue4",	"#00688B",	0},
    {"dimgray",		"#696969",	0},
    {"dimgrey",		"#696969",	0},
    {"dodgerblue",	"#1E90FF",	0},
    {"dodgerblue1",	"#1E90FF",	0},
    {"dodgerblue2",	"#1C86EE",	0},
    {"dodgerblue3",	"#1874CD",	0},
    {"dodgerblue4",	"#104E8B",	0},
    {"firebrick",	"#B22222",	0},
    {"firebrick1",	"#FF3030",	0},
    {"firebrick2",	"#EE2C2C",	0},
    {"firebrick3",	"#CD2626",	0},
    {"firebrick4",	"#8B1A1A",	0},
    {"floralwhite",	"#FFFAF0",	0},
    {"forestgreen",	"#228B22",	0},
    {"gainsboro",	"#DCDCDC",	0},
    {"ghostwhite",	"#F8F8FF",	0},
    {"gold",		"#FFD700",	0},
    {"gold1",		"#FFD700",	0},
    {"gold2",		"#EEC900",	0},
    {"gold3",		"#CDAD00",	0},
    {"gold4",		"#8B7500",	0},
    {"goldenrod",	"#DAA520",	0},
    {"goldenrod1",	"#FFC125",	0},
    {"goldenrod2",	"#EEB422",	0},
    {"goldenrod3",	"#CD9B1D",	0},
    {"goldenrod4",	"#8B6914",	0},
    {"gray",		"#BEBEBE",	0},
    {"gray0",		"#000000",	0},
    {"gray1",		"#030303",	0},
    {"gray2",		"#050505",	0},
    {"gray3",		"#080808",	0},
    {"gray4",		"#0A0A0A",	0},
    {"gray5",		"#0D0D0D",	0},
    {"gray6",		"#0F0F0F",	0},
    {"gray7",		"#121212",	0},
    {"gray8",		"#141414",	0},
    {"gray9",		"#171717",	0},
    {"gray10",		"#1A1A1A",	0},
    {"gray11",		"#1C1C1C",	0},
    {"gray12",		"#1F1F1F",	0},
    {"gray13",		"#212121",	0},
    {"gray14",		"#242424",	0},
    {"gray15",		"#262626",	0},
    {"gray16",		"#292929",	0},
    {"gray17",		"#2B2B2B",	0},
    {"gray18",		"#2E2E2E",	0},
    {"gray19",		"#303030",	0},
    {"gray20",		"#333333",	0},
    {"gray21",		"#363636",	0},
    {"gray22",		"#383838",	0},
    {"gray23",		"#3B3B3B",	0},
    {"gray24",		"#3D3D3D",	0},
    {"gray25",		"#404040",	0},
    {"gray26",		"#424242",	0},
    {"gray27",		"#454545",	0},
    {"gray28",		"#474747",	0},
    {"gray29",		"#4A4A4A",	0},
    {"gray30",		"#4D4D4D",	0},
    {"gray31",		"#4F4F4F",	0},
    {"gray32",		"#525252",	0},
    {"gray33",		"#545454",	0},
    {"gray34",		"#575757",	0},
    {"gray35",		"#595959",	0},
    {"gray36",		"#5C5C5C",	0},
    {"gray37",		"#5E5E5E",	0},
    {"gray38",		"#616161",	0},
    {"gray39",		"#636363",	0},
    {"gray40",		"#666666",	0},
    {"gray41",		"#696969",	0},
    {"gray42",		"#6B6B6B",	0},
    {"gray43",		"#6E6E6E",	0},
    {"gray44",		"#707070",	0},
    {"gray45",		"#737373",	0},
    {"gray46",		"#757575",	0},
    {"gray47",		"#787878",	0},
    {"gray48",		"#7A7A7A",	0},
    {"gray49",		"#7D7D7D",	0},
    {"gray50",		"#7F7F7F",	0},
    {"gray51",		"#828282",	0},
    {"gray52",		"#858585",	0},
    {"gray53",		"#878787",	0},
    {"gray54",		"#8A8A8A",	0},
    {"gray55",		"#8C8C8C",	0},
    {"gray56",		"#8F8F8F",	0},
    {"gray57",		"#919191",	0},
    {"gray58",		"#949494",	0},
    {"gray59",		"#969696",	0},
    {"gray60",		"#999999",	0},
    {"gray61",		"#9C9C9C",	0},
    {"gray62",		"#9E9E9E",	0},
    {"gray63",		"#A1A1A1",	0},
    {"gray64",		"#A3A3A3",	0},
    {"gray65",		"#A6A6A6",	0},
    {"gray66",		"#A8A8A8",	0},
    {"gray67",		"#ABABAB",	0},
    {"gray68",		"#ADADAD",	0},
    {"gray69",		"#B0B0B0",	0},
    {"gray70",		"#B3B3B3",	0},
    {"gray71",		"#B5B5B5",	0},
    {"gray72",		"#B8B8B8",	0},
    {"gray73",		"#BABABA",	0},
    {"gray74",		"#BDBDBD",	0},
    {"gray75",		"#BFBFBF",	0},
    {"gray76",		"#C2C2C2",	0},
    {"gray77",		"#C4C4C4",	0},
    {"gray78",		"#C7C7C7",	0},
    {"gray79",		"#C9C9C9",	0},
    {"gray80",		"#CCCCCC",	0},
    {"gray81",		"#CFCFCF",	0},
    {"gray82",		"#D1D1D1",	0},
    {"gray83",		"#D4D4D4",	0},
    {"gray84",		"#D6D6D6",	0},
    {"gray85",		"#D9D9D9",	0},
    {"gray86",		"#DBDBDB",	0},
    {"gray87",		"#DEDEDE",	0},
    {"gray88",		"#E0E0E0",	0},
    {"gray89",		"#E3E3E3",	0},
    {"gray90",		"#E5E5E5",	0},
    {"gray91",		"#E8E8E8",	0},
    {"gray92",		"#EBEBEB",	0},
    {"gray93",		"#EDEDED",	0},
    {"gray94",		"#F0F0F0",	0},
    {"gray95",		"#F2F2F2",	0},
    {"gray96",		"#F5F5F5",	0},
    {"gray97",		"#F7F7F7",	0},
    {"gray98",		"#FAFAFA",	0},
    {"gray99",		"#FCFCFC",	0},
    {"gray100",		"#FFFFFF",	0},
    {"green",		"#00FF00",	0},
    {"green1",		"#00FF00",	0},
    {"green2",		"#00EE00",	0},
    {"green3",		"#00CD00",	0},
    {"green4",		"#008B00",	0},
    {"greenyellow",	"#ADFF2F",	0},
    {"grey",		"#BEBEBE",	0},
    {"grey0",		"#000000",	0},
    {"grey1",		"#030303",	0},
    {"grey2",		"#050505",	0},
    {"grey3",		"#080808",	0},
    {"grey4",		"#0A0A0A",	0},
    {"grey5",		"#0D0D0D",	0},
    {"grey6",		"#0F0F0F",	0},
    {"grey7",		"#121212",	0},
    {"grey8",		"#141414",	0},
    {"grey9",		"#171717",	0},
    {"grey10",		"#1A1A1A",	0},
    {"grey11",		"#1C1C1C",	0},
    {"grey12",		"#1F1F1F",	0},
    {"grey13",		"#212121",	0},
    {"grey14",		"#242424",	0},
    {"grey15",		"#262626",	0},
    {"grey16",		"#292929",	0},
    {"grey17",		"#2B2B2B",	0},
    {"grey18",		"#2E2E2E",	0},
    {"grey19",		"#303030",	0},
    {"grey20",		"#333333",	0},
    {"grey21",		"#363636",	0},
    {"grey22",		"#383838",	0},
    {"grey23",		"#3B3B3B",	0},
    {"grey24",		"#3D3D3D",	0},
    {"grey25",		"#404040",	0},
    {"grey26",		"#424242",	0},
    {"grey27",		"#454545",	0},
    {"grey28",		"#474747",	0},
    {"grey29",		"#4A4A4A",	0},
    {"grey30",		"#4D4D4D",	0},
    {"grey31",		"#4F4F4F",	0},
    {"grey32",		"#525252",	0},
    {"grey33",		"#545454",	0},
    {"grey34",		"#575757",	0},
    {"grey35",		"#595959",	0},
    {"grey36",		"#5C5C5C",	0},
    {"grey37",		"#5E5E5E",	0},
    {"grey38",		"#616161",	0},
    {"grey39",		"#636363",	0},
    {"grey40",		"#666666",	0},
    {"grey41",		"#696969",	0},
    {"grey42",		"#6B6B6B",	0},
    {"grey43",		"#6E6E6E",	0},
    {"grey44",		"#707070",	0},
    {"grey45",		"#737373",	0},
    {"grey46",		"#757575",	0},
    {"grey47",		"#787878",	0},
    {"grey48",		"#7A7A7A",	0},
    {"grey49",		"#7D7D7D",	0},
    {"grey50",		"#7F7F7F",	0},
    {"grey51",		"#828282",	0},
    {"grey52",		"#858585",	0},
    {"grey53",		"#878787",	0},
    {"grey54",		"#8A8A8A",	0},
    {"grey55",		"#8C8C8C",	0},
    {"grey56",		"#8F8F8F",	0},
    {"grey57",		"#919191",	0},
    {"grey58",		"#949494",	0},
    {"grey59",		"#969696",	0},
    {"grey60",		"#999999",	0},
    {"grey61",		"#9C9C9C",	0},
    {"grey62",		"#9E9E9E",	0},
    {"grey63",		"#A1A1A1",	0},
    {"grey64",		"#A3A3A3",	0},
    {"grey65",		"#A6A6A6",	0},
    {"grey66",		"#A8A8A8",	0},
    {"grey67",		"#ABABAB",	0},
    {"grey68",		"#ADADAD",	0},
    {"grey69",		"#B0B0B0",	0},
    {"grey70",		"#B3B3B3",	0},
    {"grey71",		"#B5B5B5",	0},
    {"grey72",		"#B8B8B8",	0},
    {"grey73",		"#BABABA",	0},
    {"grey74",		"#BDBDBD",	0},
    {"grey75",		"#BFBFBF",	0},
    {"grey76",		"#C2C2C2",	0},
    {"grey77",		"#C4C4C4",	0},
    {"grey78",		"#C7C7C7",	0},
    {"grey79",		"#C9C9C9",	0},
    {"grey80",		"#CCCCCC",	0},
    {"grey81",		"#CFCFCF",	0},
    {"grey82",		"#D1D1D1",	0},
    {"grey83",		"#D4D4D4",	0},
    {"grey84",		"#D6D6D6",	0},
    {"grey85",		"#D9D9D9",	0},
    {"grey86",		"#DBDBDB",	0},
    {"grey87",		"#DEDEDE",	0},
    {"grey88",		"#E0E0E0",	0},
    {"grey89",		"#E3E3E3",	0},
    {"grey90",		"#E5E5E5",	0},
    {"grey91",		"#E8E8E8",	0},
    {"grey92",		"#EBEBEB",	0},
    {"grey93",		"#EDEDED",	0},
    {"grey94",		"#F0F0F0",	0},
    {"grey95",		"#F2F2F2",	0},
    {"grey96",		"#F5F5F5",	0},
    {"grey97",		"#F7F7F7",	0},
    {"grey98",		"#FAFAFA",	0},
    {"grey99",		"#FCFCFC",	0},
    {"grey100",		"#FFFFFF",	0},
    {"honeydew",	"#F0FFF0",	0},
    {"honeydew1",	"#F0FFF0",	0},
    {"honeydew2",	"#E0EEE0",	0},
    {"honeydew3",	"#C1CDC1",	0},
    {"honeydew4",	"#838B83",	0},
    {"hotpink",		"#FF69B4",	0},
    {"hotpink1",	"#FF6EB4",	0},
    {"hotpink2",	"#EE6AA7",	0},
    {"hotpink3",	"#CD6090",	0},
    {"hotpink4",	"#8B3A62",	0},
    {"indianred",	"#CD5C5C",	0},
    {"indianred1",	"#FF6A6A",	0},
    {"indianred2",	"#EE6363",	0},
    {"indianred3",	"#CD5555",	0},
    {"indianred4",	"#8B3A3A",	0},
    {"ivory",		"#FFFFF0",	0},
    {"ivory1",		"#FFFFF0",	0},
    {"ivory2",		"#EEEEE0",	0},
    {"ivory3",		"#CDCDC1",	0},
    {"ivory4",		"#8B8B83",	0},
    {"khaki",		"#F0E68C",	0},
    {"khaki1",		"#FFF68F",	0},
    {"khaki2",		"#EEE685",	0},
    {"khaki3",		"#CDC673",	0},
    {"khaki4",		"#8B864E",	0},
    {"lavender",	"#E6E6FA",	0},
    {"lavenderblush",	"#FFF0F5",	0},
    {"lavenderblush1",	"#FFF0F5",	0},
    {"lavenderblush2",	"#EEE0E5",	0},
    {"lavenderblush3",	"#CDC1C5",	0},
    {"lavenderblush4",	"#8B8386",	0},
    {"lawngreen",	"#7CFC00",	0},
    {"lemonchiffon",	"#FFFACD",	0},
    {"lemonchiffon1",	"#FFFACD",	0},
    {"lemonchiffon2",	"#EEE9BF",	0},
    {"lemonchiffon3",	"#CDC9A5",	0},
    {"lemonchiffon4",	"#8B8970",	0},
    {"lightblue",	"#ADD8E6",	0},
    {"lightblue1",	"#BFEFFF",	0},
    {"lightblue2",	"#B2DFEE",	0},
    {"lightblue3",	"#9AC0CD",	0},
    {"lightblue4",	"#68838B",	0},
    {"lightcoral",	"#F08080",	0},
    {"lightcyan",	"#E0FFFF",	0},
    {"lightcyan1",	"#E0FFFF",	0},
    {"lightcyan2",	"#D1EEEE",	0},
    {"lightcyan3",	"#B4CDCD",	0},
    {"lightcyan4",	"#7A8B8B",	0},
    {"lightgoldenrod",	"#EEDD82",	0},
    {"lightgoldenrod1",	"#FFEC8B",	0},
    {"lightgoldenrod2",	"#EEDC82",	0},
    {"lightgoldenrod3",	"#CDBE70",	0},
    {"lightgoldenrod4",	"#8B814C",	0},
    {"lightgoldenrodyellow","#FAFAD2",	0},
    {"lightgray",	"#D3D3D3",	0},
    {"lightgreen",	"#90EE90",	0},
    {"lightgrey",	"#D3D3D3",	0},
    {"lightpink",	"#FFB6C1",	0},
    {"lightpink1",	"#FFAEB9",	0},
    {"lightpink2",	"#EEA2AD",	0},
    {"lightpink3",	"#CD8C95",	0},
    {"lightpink4",	"#8B5F65",	0},
    {"lightsalmon",	"#FFA07A",	0},
    {"lightsalmon1",	"#FFA07A",	0},
    {"lightsalmon2",	"#EE9572",	0},
    {"lightsalmon3",	"#CD8162",	0},
    {"lightsalmon4",	"#8B5742",	0},
    {"lightseagreen",	"#20B2AA",	0},
    {"lightskyblue",	"#87CEFA",	0},
    {"lightskyblue1",	"#B0E2FF",	0},
    {"lightskyblue2",	"#A4D3EE",	0},
    {"lightskyblue3",	"#8DB6CD",	0},
    {"lightskyblue4",	"#607B8B",	0},
    {"lightslateblue",	"#8470FF",	0},
    {"lightslategray",	"#778899",	0},
    {"lightslategrey",	"#778899",	0},
    {"lightsteelblue",	"#B0C4DE",	0},
    {"lightsteelblue1",	"#CAE1FF",	0},
    {"lightsteelblue2",	"#BCD2EE",	0},
    {"lightsteelblue3",	"#A2B5CD",	0},
    {"lightsteelblue4",	"#6E7B8B",	0},
    {"lightyellow",	"#FFFFE0",	0},
    {"lightyellow1",	"#FFFFE0",	0},
    {"lightyellow2",	"#EEEED1",	0},
    {"lightyellow3",	"#CDCDB4",	0},
    {"lightyellow4",	"#8B8B7A",	0},
    {"limegreen",	"#32CD32",	0},
    {"linen",		"#FAF0E6",	0},
    {"magenta",		"#FF00FF",	0},
    {"magenta1",	"#FF00FF",	0},
    {"magenta2",	"#EE00EE",	0},
    {"magenta3",	"#CD00CD",	0},
    {"magenta4",	"#8B008B",	0},
    {"maroon",		"#B03060",	0},
    {"maroon1",		"#FF34B3",	0},
    {"maroon2",		"#EE30A7",	0},
    {"maroon3",		"#CD2990",	0},
    {"maroon4",		"#8B1C62",	0},
    {"mediumaquamarine","#66CDAA",	0},
    {"mediumblue",	"#0000CD",	0},
    {"mediumorchid",	"#BA55D3",	0},
    {"mediumorchid1",	"#E066FF",	0},
    {"mediumorchid2",	"#D15FEE",	0},
    {"mediumorchid3",	"#B452CD",	0},
    {"mediumorchid4",	"#7A378B",	0},
    {"mediumpurple",	"#9370DB",	0},
    {"mediumpurple1",	"#AB82FF",	0},
    {"mediumpurple2",	"#9F79EE",	0},
    {"mediumpurple3",	"#8968CD",	0},
    {"mediumpurple4",	"#5D478B",	0},
    {"mediumseagreen",	"#3CB371",	0},
    {"mediumslateblue",	"#7B68EE",	0},
    {"mediumspringgreen","#00FA9A",	0},
    {"mediumturquoise",	"#48D1CC",	0},
    {"mediumvioletred",	"#C71585",	0},
    {"midnightblue",	"#191970",	0},
    {"mintcream",	"#F5FFFA",	0},
    {"mistyrose",	"#FFE4E1",	0},
    {"mistyrose1",	"#FFE4E1",	0},
    {"mistyrose2",	"#EED5D2",	0},
    {"mistyrose3",	"#CDB7B5",	0},
    {"mistyrose4",	"#8B7D7B",	0},
    {"moccasin",	"#FFE4B5",	0},
    {"navajowhite",	"#FFDEAD",	0},
    {"navajowhite1",	"#FFDEAD",	0},
    {"navajowhite2",	"#EECFA1",	0},
    {"navajowhite3",	"#CDB38B",	0},
    {"navajowhite4",	"#8B795E",	0},
    {"navy",		"#000080",	0},
    {"navyblue",	"#000080",	0},
    {"oldlace",		"#FDF5E6",	0},
    {"olivedrab",	"#6B8E23",	0},
    {"olivedrab1",	"#C0FF3E",	0},
    {"olivedrab2",	"#B3EE3A",	0},
    {"olivedrab3",	"#9ACD32",	0},
    {"olivedrab4",	"#698B22",	0},
    {"orange",		"#FFA500",	0},
    {"orange1",		"#FFA500",	0},
    {"orange2",		"#EE9A00",	0},
    {"orange3",		"#CD8500",	0},
    {"orange4",		"#8B5A00",	0},
    {"orangered",	"#FF4500",	0},
    {"orangered1",	"#FF4500",	0},
    {"orangered2",	"#EE4000",	0},
    {"orangered3",	"#CD3700",	0},
    {"orangered4",	"#8B2500",	0},
    {"orchid",		"#DA70D6",	0},
    {"orchid1",		"#FF83FA",	0},
    {"orchid2",		"#EE7AE9",	0},
    {"orchid3",		"#CD69C9",	0},
    {"orchid4",		"#8B4789",	0},
    {"palegoldenrod",	"#EEE8AA",	0},
    {"palegreen",	"#98FB98",	0},
    {"palegreen1",	"#9AFF9A",	0},
    {"palegreen2",	"#90EE90",	0},
    {"palegreen3",	"#7CCD7C",	0},
    {"palegreen4",	"#548B54",	0},
    {"paleturquoise",	"#AFEEEE",	0},
    {"paleturquoise1",	"#BBFFFF",	0},
    {"paleturquoise2",	"#AEEEEE",	0},
    {"paleturquoise3",	"#96CDCD",	0},
    {"paleturquoise4",	"#668B8B",	0},
    {"palevioletred",	"#DB7093",	0},
    {"palevioletred1",	"#FF82AB",	0},
    {"palevioletred2",	"#EE799F",	0},
    {"palevioletred3",	"#CD6889",	0},
    {"palevioletred4",	"#8B475D",	0},
    {"papayawhip",	"#FFEFD5",	0},
    {"peachpuff",	"#FFDAB9",	0},
    {"peachpuff1",	"#FFDAB9",	0},
    {"peachpuff2",	"#EECBAD",	0},
    {"peachpuff3",	"#CDAF95",	0},
    {"peachpuff4",	"#8B7765",	0},
    {"peru",		"#CD853F",	0},
    {"pink",		"#FFC0CB",	0},
    {"pink1",		"#FFB5C5",	0},
    {"pink2",		"#EEA9B8",	0},
    {"pink3",		"#CD919E",	0},
    {"pink4",		"#8B636C",	0},
    {"plum",		"#DDA0DD",	0},
    {"plum1",		"#FFBBFF",	0},
    {"plum2",		"#EEAEEE",	0},
    {"plum3",		"#CD96CD",	0},
    {"plum4",		"#8B668B",	0},
    {"powderblue",	"#B0E0E6",	0},
    {"purple",		"#A020F0",	0},
    {"purple1",		"#9B30FF",	0},
    {"purple2",		"#912CEE",	0},
    {"purple3",		"#7D26CD",	0},
    {"purple4",		"#551A8B",	0},
    {"red",		"#FF0000",	0},
    {"red1",		"#FF0000",	0},
    {"red2",		"#EE0000",	0},
    {"red3",		"#CD0000",	0},
    {"red4",		"#8B0000",	0},
    {"rosybrown",	"#BC8F8F",	0},
    {"rosybrown1",	"#FFC1C1",	0},
    {"rosybrown2",	"#EEB4B4",	0},
    {"rosybrown3",	"#CD9B9B",	0},
    {"rosybrown4",	"#8B6969",	0},
    {"royalblue",	"#4169E1",	0},
    {"royalblue1",	"#4876FF",	0},
    {"royalblue2",	"#436EEE",	0},
    {"royalblue3",	"#3A5FCD",	0},
    {"royalblue4",	"#27408B",	0},
    {"saddlebrown",	"#8B4513",	0},
    {"salmon",		"#FA8072",	0},
    {"salmon1",		"#FF8C69",	0},
    {"salmon2",		"#EE8262",	0},
    {"salmon3",		"#CD7054",	0},
    {"salmon4",		"#8B4C39",	0},
    {"sandybrown",	"#F4A460",	0},
    {"seagreen",	"#2E8B57",	0},
    {"seagreen1",	"#54FF9F",	0},
    {"seagreen2",	"#4EEE94",	0},
    {"seagreen3",	"#43CD80",	0},
    {"seagreen4",	"#2E8B57",	0},
    {"seashell",	"#FFF5EE",	0},
    {"seashell1",	"#FFF5EE",	0},
    {"seashell2",	"#EEE5DE",	0},
    {"seashell3",	"#CDC5BF",	0},
    {"seashell4",	"#8B8682",	0},
    {"sienna",		"#A0522D",	0},
    {"sienna1",		"#FF8247",	0},
    {"sienna2",		"#EE7942",	0},
    {"sienna3",		"#CD6839",	0},
    {"sienna4",		"#8B4726",	0},
    {"skyblue",		"#87CEEB",	0},
    {"skyblue1",	"#87CEFF",	0},
    {"skyblue2",	"#7EC0EE",	0},
    {"skyblue3",	"#6CA6CD",	0},
    {"skyblue4",	"#4A708B",	0},
    {"slateblue",	"#6A5ACD",	0},
    {"slateblue1",	"#836FFF",	0},
    {"slateblue2",	"#7A67EE",	0},
    {"slateblue3",	"#6959CD",	0},
    {"slateblue4",	"#473C8B",	0},
    {"slategray",	"#708090",	0},
    {"slategray1",	"#C6E2FF",	0},
    {"slategray2",	"#B9D3EE",	0},
    {"slategray3",	"#9FB6CD",	0},
    {"slategray4",	"#6C7B8B",	0},
    {"slategrey",	"#708090",	0},
    {"snow",		"#FFFAFA",	0},
    {"snow1",		"#FFFAFA",	0},
    {"snow2",		"#EEE9E9",	0},
    {"snow3",		"#CDC9C9",	0},
    {"snow4",		"#8B8989",	0},
    {"springgreen",	"#00FF7F",	0},
    {"springgreen1",	"#00FF7F",	0},
    {"springgreen2",	"#00EE76",	0},
    {"springgreen3",	"#00CD66",	0},
    {"springgreen4",	"#008B45",	0},
    {"steelblue",	"#4682B4",	0},
    {"steelblue1",	"#63B8FF",	0},
    {"steelblue2",	"#5CACEE",	0},
    {"steelblue3",	"#4F94CD",	0},
    {"steelblue4",	"#36648B",	0},
    {"tan",		"#D2B48C",	0},
    {"tan1",		"#FFA54F",	0},
    {"tan2",		"#EE9A49",	0},
    {"tan3",		"#CD853F",	0},
    {"tan4",		"#8B5A2B",	0},
    {"thistle",		"#D8BFD8",	0},
    {"thistle1",	"#FFE1FF",	0},
    {"thistle2",	"#EED2EE",	0},
    {"thistle3",	"#CDB5CD",	0},
    {"thistle4",	"#8B7B8B",	0},
    {"tomato",		"#FF6347",	0},
    {"tomato1",		"#FF6347",	0},
    {"tomato2",		"#EE5C42",	0},
    {"tomato3",		"#CD4F39",	0},
    {"tomato4",		"#8B3626",	0},
    {"turquoise",	"#40E0D0",	0},
    {"turquoise1",	"#00F5FF",	0},
    {"turquoise2",	"#00E5EE",	0},
    {"turquoise3",	"#00C5CD",	0},
    {"turquoise4",	"#00868B",	0},
    {"violet",		"#EE82EE",	0},
    {"violetred",	"#D02090",	0},
    {"violetred1",	"#FF3E96",	0},
    {"violetred2",	"#EE3A8C",	0},
    {"violetred3",	"#CD3278",	0},
    {"violetred4",	"#8B2252",	0},
    {"wheat",		"#F5DEB3",	0},
    {"wheat1",		"#FFE7BA",	0},
    {"wheat2",		"#EED8AE",	0},
    {"wheat3",		"#CDBA96",	0},
    {"wheat4",		"#8B7E66",	0},
    {"whitesmoke",	"#F5F5F5",	0},
    {"yellow",		"#FFFF00",	0},
    {"yellow1",		"#FFFF00",	0},
    {"yellow2",		"#EEEE00",	0},
    {"yellow3",		"#CDCD00",	0},
    {"yellow4",		"#8B8B00",	0},
    {"yellowgreen",	"#9ACD32",	0},
    {NULL,		NULL,		0}
};


int R_ColorTableSize;
unsigned int R_ColorTable[COLOR_TABLE_SIZE];

/* Hex Digit to Integer Conversion */

static unsigned int hexdigit(int digit)
{
    if('0' <= digit && digit <= '9') return digit - '0';
    if('A' <= digit && digit <= 'F') return 10 + digit - 'A';
    if('a' <= digit && digit <= 'f') return 10 + digit - 'a';
    /*else */ error("invalid hex digit in color or lty");
    return digit; /* never occurs (-Wall) */
}

#ifdef UNUSED
/* Integer to Hex Digit */

static unsigned int digithex(int digit)
{
    return HexDigits[abs(digit) % 16];
}
#endif


/* String Comparison Ignoring Case and Squeezing Out Blanks */
int StrMatch(char *s, char *t)
{
    for(;;) {
	if(*s == '\0' && *t == '\0') {
	    return 1;
	}
	if(*s == ' ') {
	    s++; continue;
	}
	if(*t == ' ') {
	    t++; continue;
	}
	if(tolower(*s++) != tolower(*t++))
	    return 0;
    }
}


/* #RRGGBB String to Internal Color Code */

unsigned int rgb2col(char *rgb)
{
    unsigned int r, g, b;
    if(rgb[0] != '#' || strlen(rgb) != 7)
	error("invalid RGB specification");
    r = 16 * hexdigit(rgb[1]) + hexdigit(rgb[2]);
    g = 16 * hexdigit(rgb[3]) + hexdigit(rgb[4]);
    b = 16 * hexdigit(rgb[5]) + hexdigit(rgb[6]);
    return R_RGB(r, g, b);
}

/* External Color Name to Internal Color Code */

unsigned int name2col(char *nm)
{
    int i;
    if(strcmp(nm, "NA") == 0 || strcmp(nm, "transparent") == 0)
	return NA_INTEGER;
    for(i = 0; ColorDataBase[i].name ; i++) {
	if(StrMatch(ColorDataBase[i].name, nm))
	    return ColorDataBase[i].code;
    }
    error("invalid color name");
    return 0;		/* never occurs but avoid compiler warnings */
}

/* Index (as string) to Internal Color Code */

unsigned int number2col(char *nm)
{
    int indx;
    char *ptr;
    indx = strtod(nm, &ptr);
    if(*ptr) error("invalid color specification");
    if(indx == 0) return Rf_dpptr(CurrentDevice())->bg;
    else return R_ColorTable[(indx-1) % R_ColorTableSize];
}


static char ColBuf[8];

char *RGB2rgb(unsigned int r, unsigned int g, unsigned int b)
{
    ColBuf[0] = '#';
    ColBuf[1] = HexDigits[(r >> 4) & 15];
    ColBuf[2] = HexDigits[r & 15];
    ColBuf[3] = HexDigits[(g >> 4) & 15];
    ColBuf[4] = HexDigits[g & 15];
    ColBuf[5] = HexDigits[(b >> 4) & 15];
    ColBuf[6] = HexDigits[b & 15];
    ColBuf[7] = '\0';
    return &ColBuf[0];
}


/* Internal to External Color Representation */
/* Search the color name database first */
/* If this fails, create an #RRGGBB string */

char *col2name(unsigned int col)
{
    int i;

    if(R_ALPHA(col) != 0) return "transparent";
    for(i=0 ; ColorDataBase[i].name ; i++) {
	if(col == ColorDataBase[i].code)
	    return ColorDataBase[i].name;
    }
    ColBuf[0] = '#';
    ColBuf[1] = HexDigits[(col >>  4) & 15];
    ColBuf[2] = HexDigits[(col	    ) & 15];
    ColBuf[3] = HexDigits[(col >> 12) & 15];
    ColBuf[4] = HexDigits[(col >>  8) & 15];
    ColBuf[5] = HexDigits[(col >> 20) & 15];
    ColBuf[6] = HexDigits[(col >> 16) & 15];
    ColBuf[7] = '\0';
    return &ColBuf[0];
}

/* NOTE that this is called with dd == NULL by */
/* the initialisation code in which case, str2col */
/* assumes that `s' is a name */

unsigned int str2col(char *s)
{
    if(s[0] == '#') return rgb2col(s);
    else if(isdigit((int)s[0])) return number2col(s);
    else return name2col(s);
}

/* Convert a sexp element to an R  color desc */
/* We Assume that Checks Have Been Done */

unsigned int RGBpar(SEXP x, int i)
{
    int indx;
    if(isString(x)) {
	return str2col(CHAR(STRING_ELT(x, i)));
    }
    else if(isInteger(x) || isLogical(x)) {
	if(INTEGER(x)[i] == NA_INTEGER) return NA_INTEGER;
	indx = INTEGER(x)[i] - 1;
	if(indx < 0) return Rf_dpptr(CurrentDevice())->bg;
	else return R_ColorTable[indx % R_ColorTableSize];
    }
    else if(isReal(x)) {
	if(!R_FINITE(REAL(x)[i])) return NA_INTEGER;
	indx = REAL(x)[i] - 1;
	if(indx < 0) return Rf_dpptr(CurrentDevice())->bg;
	else return R_ColorTable[indx % R_ColorTableSize];
    }
    return 0;		/* should not occur */
}


/* Initialize the Color Databases */

void InitColors(void)
{
    int i;

    /* Initialize the Color Database */
    for(i=0 ; ColorDataBase[i].name ; i++)
	ColorDataBase[i].code = rgb2col(ColorDataBase[i].rgb);
    ColorDataBaseSize = i;

    /* Install Default Palette */
    for(i=0 ; DefaultPalette[i] ; i++)
	R_ColorTable[i] = str2col(DefaultPalette[i]);
    R_ColorTableSize = i;
}

/*  LINE TEXTURE CODE */

/*
 *  LINE TEXTURE SPECIFICATION
 *
 *  Linetypes are stored internally in integers.  An integer
 *  is interpreted as containing a sequence of 8 4-bit integers
 *  which give the lengths of up to 8 on-off line segments.
 *  The lengths are typically interpreted as pixels on a screen
 *  and as "points" in postscript.
 *
 *  more comments (and LTY_* def.s) in	../include/Rgraphics.h
 *					----------------------
 */

typedef struct {
    char *name;
    unsigned int pattern;
} LineTYPE;

static LineTYPE linetype[] = {
    { "blank",   LTY_BLANK   },/* 0 */
    { "solid",	 LTY_SOLID   },/* 1 */
    { "dashed",	 LTY_DASHED  },/* 2 */
    { "dotted",	 LTY_DOTTED  },/* 3 */
    { "dotdash", LTY_DOTDASH },/* 4 */
    { "longdash",LTY_LONGDASH},/* 5 */
    { "twodash", LTY_TWODASH },/* 6 */
    { NULL,	 0	     },
};

static int nlinetype = (sizeof(linetype)/sizeof(LineTYPE)-2);

unsigned int LTYpar(SEXP value, int ind)
{
    char *p;
    int i, code, shift, digit, len;
    double rcode;

    if(isString(value)) {
	for(i = 0; linetype[i].name; i++) { /* is it the i-th name ? */
	    if(!strcmp(CHAR(STRING_ELT(value, ind)), linetype[i].name))
		return linetype[i].pattern;
	}
	/* otherwise, a string of hex digits: */
	code = 0;
	shift = 0;
	p = CHAR(STRING_ELT(value, ind));
	len = strlen(p);
	if(len < 2 || len > 8 || len % 2 == 1)
	    error("invalid line type: must be length 2, 4, 6 or 8");
	for(; *p; p++) {
	    digit = hexdigit(*p);
	    code  |= (digit<<shift);
	    shift += 4;
	}
	return code;
    }
    else if(isInteger(value)) {
	code = INTEGER(value)[ind];
	if(code == NA_INTEGER || code < 0) error("invalid line type");
	if (code > 0)
	    code = (code-1) % nlinetype + 1;
	return linetype[code].pattern;
    }
    else if(isReal(value)) {
	rcode = REAL(value)[ind];
	if(!R_FINITE(rcode) || rcode < 0) error("invalid line type");
	code = rcode;
	if (code > 0)
	    code = (code-1) % nlinetype + 1;
	return linetype[code].pattern;
    }
    else {
	error("invalid line type"); /*NOTREACHED, for -Wall : */ return 0;
    }
}
#undef LTY_do_int

SEXP LTYget(unsigned int lty)
{
    SEXP ans;
    int i, ndash;
    unsigned char dash[8];
    unsigned int l;

    for (i = 0; linetype[i].name; i++) {
	if(linetype[i].pattern == lty)
	    return mkString(linetype[i].name);
    }

    l = lty; ndash = 0;
    for (i = 0; i < 8 && l & 15; i++) {
	dash[ndash++] = l&15;
	l = l >> 4;
    }
    PROTECT(ans = allocVector(STRSXP, 1));
    SET_STRING_ELT(ans, 0, allocString(ndash));
    for(i=0 ; i<ndash ; i++) {
	CHAR(STRING_ELT(ans, 0))[i] = HexDigits[dash[i]];
    }
    CHAR(STRING_ELT(ans,0))[ndash] = '\0';
    UNPROTECT(1);
    return ans;
}

/*
 *  DEVICE FUNCTIONS
 *
 *  R allows there to be (up to 64) multiple devices in
 *  existence at the same time.	 Only one device is the
 *  active device and all drawing occurs in this device
 *
 *  Each device has its own set of graphics parameters
 *  so that switching between devices, switches between
 *  their graphical contexts (e.g., if you set the line
 *  width on one device then switch to another device,
 *  don't expect to be using the line width you just set!)
 *
 *  Each device has additional device-specific graphics
 *  parameters which the device driver (i.e., NOT this
 *  generic graphics code) is wholly responsible for
 *  maintaining (including creating and destroying special
 *  resources such as X11 windows).
 *
 *  Each device has a display list which records every
 *  graphical operation since the last Rf_dpptr(dd)->newPage;
 *  this is used to redraw the output on the device
 *  when it is resized and to copy output from one device
 *  to another (this can be disabled, which is the default
 *  for postscript).
 *
 *  NOTE: that graphical operations should only be
 *  recorded in the displayList if they are "guaranteed"
 *  to succeed (to avoid heaps of error messages on a
 *  redraw) which means that the recording should be the
 *  last thing done in a graphical operation (see do_*
 *  in plot.c).
 *
 */

void DevNull(void) {}

static int R_CurrentDevice = 0;
int R_NumDevices = 1;
DevDesc* R_Devices[R_MaxDevices];
static int R_LastDeviceEntry = R_MaxDevices - 1;  /* used to catch creation
						     of too many devices */
static int R_MaxRegularDevices =  R_MaxDevices - 1; /* not coulting the
						       LastDeivceEntry */

/* a dummy description to point to when there are no active devices */

DevDesc nullDevice;

/* unused
void devError(void)
{
    error("No graphics device is active -- "
	  "SHOULDN'T happen anymore -- please report");
}
*/

int NoDevices(void)
{
    return (R_NumDevices == 1 || R_CurrentDevice == 0);
}


int NumDevices(void)
{
    return R_NumDevices;
}


DevDesc* CurrentDevice(void)
{
    return R_Devices[R_CurrentDevice];
}


DevDesc* GetDevice(int i)
{
    return R_Devices[i];
}


void R_CheckDeviceAvailable(void)
{
    if (R_NumDevices >= R_MaxRegularDevices)
	error("too many open devices");
}

Rboolean R_CheckDeviceAvailableBool(void)
{
    if (R_NumDevices >= R_MaxRegularDevices) return FALSE;
    else return TRUE;
}

void InitGraphics(void)
{
    int i;
    SEXP s, t;

    /* init R_Devices */
    R_Devices[0] = &nullDevice;
    for (i = 1; i < R_MaxDevices; i++)
	R_Devices[i] = NULL;

    /* init .Device and .Devices */
    PROTECT(s = mkString("null device"));
    gsetVar(install(".Device"), s, R_NilValue);
    PROTECT(t = mkString("null device"));
    gsetVar(install(".Devices"), CONS(t, R_NilValue), R_NilValue);
    UNPROTECT(2);

    /* Register the base graphics system with the graphics engine
     */
    registerBase();
}


static SEXP getSymbolValue(char *symbolName)
{
    SEXP t;
    t = findVar(install(symbolName), R_NilValue);
    return t;
}


int curDevice(void)
{
    return R_CurrentDevice;
}


int nextDevice(int from)
{
    if (R_NumDevices == 1)
	return 0;
    else {
	int i = from;
	int nextDev = 0;
	while ((i < (R_MaxDevices-1)) && (nextDev == 0))
	    if (R_Devices[++i] != NULL)
		nextDev = i;
	if (nextDev == 0) {
	    /* start again from 1 */
	    i = 0;
	    while (nextDev == 0)
		if (R_Devices[++i] != NULL)
		    nextDev = i;
	}
	return nextDev;
    }
}


int prevDevice(int from)
{
    if (R_NumDevices == 1)
	return 0;
    else {
	int i = from;
	int prevDev = 0;
	while ((i > 1) && (prevDev == 0))
	    if (R_Devices[--i] != NULL)
		prevDev = i;
	if (prevDev == 0) {
	    /* start again from R_MaxDevices */
	    i = R_MaxDevices;
	    while (prevDev == 0)
		if (R_Devices[--i] != NULL)
		    prevDev = i;
	}
	return prevDev;
    }
}


void addDevice(DevDesc *dd)
{
    int i;
    Rboolean appnd;
    SEXP s, t;
    DevDesc *oldd;
    PROTECT(s = getSymbolValue(".Devices"));

    if (!NoDevices())  {
	oldd = CurrentDevice();
	if (oldd->newDevStruct)
	    ((GEDevDesc*) oldd)->dev->deactivate(((GEDevDesc*) oldd)->dev);
	else
	    oldd->dp.deactivate(oldd);
    }

    /* find empty slot for new descriptor */
    i = 1;
    if (CDR(s) == R_NilValue)
	appnd = TRUE;
    else {
	s = CDR(s);
	appnd = FALSE;
    }
    while (R_Devices[i] != NULL) {
	i++;
	if (CDR(s) == R_NilValue)
	    appnd = TRUE;
	else
	    s = CDR(s);
    }
    R_CurrentDevice = i;
    R_NumDevices += 1;
    R_Devices[i] = dd;

    if (dd->newDevStruct) {
	GEregisterWithDevice((GEDevDesc*) dd);
	((GEDevDesc*) dd)->dev->activate(((GEDevDesc*) dd)->dev);
    }
    else
	Rf_dpptr(dd)->activate(dd);

    /* maintain .Devices (.Device has already been set) */
    PROTECT(t = mkString(CHAR(STRING_ELT(getSymbolValue(".Device"), 0))));
    if (appnd)
	SETCDR(s, CONS(t, R_NilValue));
    else
	SETCAR(s, t);

    UNPROTECT(2);

    copyGPar(Rf_dpptr(dd), Rf_gpptr(dd));
    GReset(dd);

    /* In case a device driver did not call R_CheckDeviceAvailable
       before starting its allocation, we complete the allocation and
       then call killDevice here.  This insures that the device gets a
       chance to deallocate its resources and the current active
       device is restored to a sane value. */
    if (i == R_LastDeviceEntry) {
        killDevice(i);
        error("too many devices open");
    }
}

/* This should be called if you have a DevDesc or a GEDevDesc
 * and you want to find the corresponding device number
 */
int deviceNumber(DevDesc *dd)
{
    int i;
    for (i = 1; i < R_MaxDevices; i++)
	if (R_Devices[i] == dd)
	    return i;
    return 0;
}

/* This should be called if you have a NewDevDesc
 * and you want to find the corresponding device number
 */
int devNumber(DevDesc *dd)
{
    int i;
    for (i = 1; i < R_MaxDevices; i++)
	if (R_Devices[i] != NULL &&
	    R_Devices[i]->newDevStruct &&
	    ((GEDevDesc*) R_Devices[i])->dev == (NewDevDesc*) dd)
	    return i;
    return 0;
}

int selectDevice(int devNum)
{
    /* valid to select nullDevice */
    if ((devNum >= 0) &&
	(devNum < R_MaxDevices) &&
	(R_Devices[devNum] != NULL)) {
	DevDesc *oldd, *dd;

	if (!NoDevices()) {
	    oldd = CurrentDevice();
	    if (oldd->newDevStruct)
		((GEDevDesc*) oldd)->dev->deactivate(((GEDevDesc *) oldd)->dev);
	    else
		oldd->dp.deactivate(oldd);
	}

	R_CurrentDevice = devNum;

	/* maintain .Device */
	gsetVar(install(".Device"),
		elt(getSymbolValue(".Devices"), devNum),
		R_NilValue);

	dd = CurrentDevice();
	if (!NoDevices()) {
	    if (dd->newDevStruct)
		((GEDevDesc*) dd)->dev->activate(((GEDevDesc*) dd)->dev);
	    else
		Rf_dpptr(dd)->activate(dd);
	}
	if (dd->newDevStruct)
	    ;
	else {
	    copyGPar(Rf_dpptr(dd), Rf_gpptr(dd));
	    GReset(dd);
	}
	return devNum;
    }
    else
	return selectDevice(nextDevice(devNum));
}

static
void removeDevice(int devNum)
{
    if ((devNum > 0) &&
	(devNum < R_MaxDevices) &&
	(R_Devices[devNum] != NULL)) {
	int i;
	SEXP s;

	if (R_Devices[devNum]->newDevStruct)
	    GEdestroyDevDesc((GEDevDesc*) R_Devices[devNum]);
	else
	    free(R_Devices[devNum]);
	R_Devices[devNum] = NULL;
	R_NumDevices -= 1;

	/* maintain .Devices */
	PROTECT(s = getSymbolValue(".Devices"));
	for (i=0; i<devNum; i++)
	    s = CDR(s);
	SETCAR(s, mkString(""));
	UNPROTECT(1);

	/* determine new current device */
	if (devNum == R_CurrentDevice) {
	    DevDesc *dd;

	    R_CurrentDevice = nextDevice(R_CurrentDevice);

	    /* maintain .Device */
	    gsetVar(install(".Device"),
		    elt(getSymbolValue(".Devices"),
			R_CurrentDevice),
		    R_NilValue);

	    dd = CurrentDevice();
	    if (!NoDevices()) {
		if (dd->newDevStruct)
		    ((GEDevDesc*) dd)->dev->activate(((GEDevDesc*) dd)->dev);
		else
		    Rf_dpptr(dd)->activate(dd);
	    }
	    copyGPar(Rf_dpptr(dd), Rf_gpptr(dd));
	    GReset(dd);
	}
    }
}



void KillDevice(DevDesc *dd)
{
    if (dd->newDevStruct)
	((GEDevDesc*) dd)->dev->close(((GEDevDesc*) dd)->dev);
    else
	Rf_dpptr(dd)->close(dd);
    removeDevice(deviceNumber(dd));
}


void killDevice(int devNum)
{
    if (!NoDevices() &&
	(devNum > 0) &&
	(devNum < R_MaxDevices)) {
	DevDesc *dd = R_Devices[devNum];
	if (dd != NULL) {
	    if (dd->newDevStruct)
		((GEDevDesc*) dd)->dev->close(((GEDevDesc*) dd)->dev);
	    else
		Rf_dpptr(dd)->close(dd);
	    removeDevice(devNum);
	}
    }
}


void KillAllDevices(void)
{
    /* don't try to close or remove the null device ! */
    while (R_NumDevices > 1)
	killDevice(R_CurrentDevice);
    /* FIXME: There should really be a formal graphics finaliser
     * but this is a good proxy for now.
     */
    GEunregisterSystem(baseRegisterIndex);
}


/* Code for maintaining DISPLAY LISTS  (Generic list code from ./list.c) */

void initDisplayList(DevDesc *dd)
{
    /* init saveParams */
    copyGPar(Rf_dpptr(dd), Rf_dpSavedptr(dd));
    if (dd->newDevStruct)
	((GEDevDesc*) dd)->dev->displayList = R_NilValue;
    else {
	dd->displayList = R_NilValue;
    }
}


void recordGraphicOperation(SEXP op, SEXP args, DevDesc *dd)
{
    if (dd->newDevStruct) {
	SEXP lastOperation = lastElt(((GEDevDesc*) dd)->dev->displayList);
	if (((GEDevDesc*) dd)->dev->displayListOn) {
	    SEXP newOperation = CONS(op, args);
	    if (lastOperation == R_NilValue)
		((GEDevDesc*) dd)->dev->displayList = CONS(newOperation,
						       R_NilValue);
	    else
		SETCDR(lastOperation, CONS(newOperation, R_NilValue));
	}
    }
    else {
	SEXP lastOperation = lastElt(dd->displayList);
	if (dd->displayListOn) {
	    SEXP newOperation = CONS(op, args);
	    if (lastOperation == R_NilValue)
		dd->displayList = CONS(newOperation, R_NilValue);
	    else
		SETCDR(lastOperation, CONS(newOperation, R_NilValue));
	}
    }
}

/* NOTE this is not declared static because it is also used in
 * base.c
 * Once graphics.c gets hacked to pieces and split into engine.c and base.c
 * then this can be made static again.
 */
void restoredpSaved(DevDesc *dd)
{
    /* NOTE that not all params should be restored before playing */
    /* the display list (e.g., don't restore the device size) */

    int i, j;

    /* do NOT restore basic device driver properties;  they are */
    /* either meant to be different (e.g., left, right, bottom, top */
    /* changed because of window resize) or never change (e.g., ipr) */

    Rf_dpptr(dd)->state = Rf_dpSavedptr(dd)->state;
    Rf_dpptr(dd)->adj = Rf_dpSavedptr(dd)->adj;
    Rf_dpptr(dd)->ann = Rf_dpSavedptr(dd)->ann;
    Rf_dpptr(dd)->bg = Rf_dpSavedptr(dd)->bg;
    Rf_dpptr(dd)->bty = Rf_dpSavedptr(dd)->bty;
    Rf_dpptr(dd)->cex = Rf_dpSavedptr(dd)->cex;
    Rf_dpptr(dd)->col = Rf_dpSavedptr(dd)->col;
    Rf_dpptr(dd)->crt = Rf_dpSavedptr(dd)->crt;
    Rf_dpptr(dd)->err = Rf_dpSavedptr(dd)->err;
    Rf_dpptr(dd)->fg = Rf_dpSavedptr(dd)->fg;
    Rf_dpptr(dd)->font = Rf_dpSavedptr(dd)->font;
    Rf_dpptr(dd)->gamma = Rf_dpSavedptr(dd)->gamma;
    Rf_dpptr(dd)->lab[0] = Rf_dpSavedptr(dd)->lab[0];
    Rf_dpptr(dd)->lab[1] = Rf_dpSavedptr(dd)->lab[1];
    Rf_dpptr(dd)->lab[2] = Rf_dpSavedptr(dd)->lab[2];
    Rf_dpptr(dd)->las = Rf_dpSavedptr(dd)->las;
    Rf_dpptr(dd)->lty = Rf_dpSavedptr(dd)->lty;
    Rf_dpptr(dd)->lwd = Rf_dpSavedptr(dd)->lwd;
    Rf_dpptr(dd)->mgp[0] = Rf_dpSavedptr(dd)->mgp[0];
    Rf_dpptr(dd)->mgp[1] = Rf_dpSavedptr(dd)->mgp[1];
    Rf_dpptr(dd)->mgp[2] = Rf_dpSavedptr(dd)->mgp[2];
    Rf_dpptr(dd)->mkh = Rf_dpSavedptr(dd)->mkh;
    Rf_dpptr(dd)->pch = Rf_dpSavedptr(dd)->pch;
    Rf_dpptr(dd)->ps = Rf_dpSavedptr(dd)->ps; /*was commented out --why?*/
    Rf_dpptr(dd)->smo = Rf_dpSavedptr(dd)->smo;
    Rf_dpptr(dd)->srt = Rf_dpSavedptr(dd)->srt;
    Rf_dpptr(dd)->tck = Rf_dpSavedptr(dd)->tck;
    Rf_dpptr(dd)->tcl = Rf_dpSavedptr(dd)->tcl;
    Rf_dpptr(dd)->tmag = Rf_dpSavedptr(dd)->tmag;
    Rf_dpptr(dd)->type = Rf_dpSavedptr(dd)->type;
    Rf_dpptr(dd)->xaxp[0] = Rf_dpSavedptr(dd)->xaxp[0];
    Rf_dpptr(dd)->xaxp[1] = Rf_dpSavedptr(dd)->xaxp[1];
    Rf_dpptr(dd)->xaxp[2] = Rf_dpSavedptr(dd)->xaxp[2];
    Rf_dpptr(dd)->xaxs = Rf_dpSavedptr(dd)->xaxs;
    Rf_dpptr(dd)->xaxt = Rf_dpSavedptr(dd)->xaxt;
    Rf_dpptr(dd)->xpd = Rf_dpSavedptr(dd)->xpd;
    Rf_dpptr(dd)->xlog = Rf_dpSavedptr(dd)->xlog;
    Rf_dpptr(dd)->yaxp[0] = Rf_dpSavedptr(dd)->yaxp[0];
    Rf_dpptr(dd)->yaxp[1] = Rf_dpSavedptr(dd)->yaxp[1];
    Rf_dpptr(dd)->yaxp[2] = Rf_dpSavedptr(dd)->yaxp[2];
    Rf_dpptr(dd)->yaxs = Rf_dpSavedptr(dd)->yaxs;
    Rf_dpptr(dd)->yaxt = Rf_dpSavedptr(dd)->yaxt;
    Rf_dpptr(dd)->ylog = Rf_dpSavedptr(dd)->ylog;
    Rf_dpptr(dd)->cexbase = Rf_dpSavedptr(dd)->cexbase;
    Rf_dpptr(dd)->cexmain = Rf_dpSavedptr(dd)->cexmain;
    Rf_dpptr(dd)->cexlab = Rf_dpSavedptr(dd)->cexlab;
    Rf_dpptr(dd)->cexsub = Rf_dpSavedptr(dd)->cexsub;
    Rf_dpptr(dd)->cexaxis = Rf_dpSavedptr(dd)->cexaxis;
    Rf_dpptr(dd)->fontmain = Rf_dpSavedptr(dd)->fontmain;
    Rf_dpptr(dd)->fontlab = Rf_dpSavedptr(dd)->fontlab;
    Rf_dpptr(dd)->fontsub = Rf_dpSavedptr(dd)->fontsub;
    Rf_dpptr(dd)->fontaxis = Rf_dpSavedptr(dd)->fontaxis;
    Rf_dpptr(dd)->colmain = Rf_dpSavedptr(dd)->colmain;
    Rf_dpptr(dd)->collab = Rf_dpSavedptr(dd)->collab;
    Rf_dpptr(dd)->colsub = Rf_dpSavedptr(dd)->colsub;
    Rf_dpptr(dd)->colaxis = Rf_dpSavedptr(dd)->colaxis;

    /* must restore layout parameters;	the different graphics */
    /* regions and coordinate transformations will be recalculated */
    /* but they need all of the layout information restored for this */
    /* to happen correctly */

    Rf_dpptr(dd)->devmode = Rf_dpSavedptr(dd)->devmode;
    Rf_dpptr(dd)->fig[0] = Rf_dpSavedptr(dd)->fig[0];
    Rf_dpptr(dd)->fig[1] = Rf_dpSavedptr(dd)->fig[1];
    Rf_dpptr(dd)->fig[2] = Rf_dpSavedptr(dd)->fig[2];
    Rf_dpptr(dd)->fig[3] = Rf_dpSavedptr(dd)->fig[3];
    Rf_dpptr(dd)->fin[0] = Rf_dpSavedptr(dd)->fin[0];
    Rf_dpptr(dd)->fin[1] = Rf_dpSavedptr(dd)->fin[1];
    Rf_dpptr(dd)->fUnits = Rf_dpSavedptr(dd)->fUnits;
    Rf_dpptr(dd)->defaultFigure = Rf_dpSavedptr(dd)->defaultFigure;
    Rf_dpptr(dd)->mar[0] = Rf_dpSavedptr(dd)->mar[0];
    Rf_dpptr(dd)->mar[1] = Rf_dpSavedptr(dd)->mar[1];
    Rf_dpptr(dd)->mar[2] = Rf_dpSavedptr(dd)->mar[2];
    Rf_dpptr(dd)->mar[3] = Rf_dpSavedptr(dd)->mar[3];
    Rf_dpptr(dd)->mai[0] = Rf_dpSavedptr(dd)->mai[0];
    Rf_dpptr(dd)->mai[1] = Rf_dpSavedptr(dd)->mai[1];
    Rf_dpptr(dd)->mai[2] = Rf_dpSavedptr(dd)->mai[2];
    Rf_dpptr(dd)->mai[3] = Rf_dpSavedptr(dd)->mai[3];
    Rf_dpptr(dd)->mUnits = Rf_dpSavedptr(dd)->mUnits;
    Rf_dpptr(dd)->mex = Rf_dpSavedptr(dd)->mex;
    Rf_dpptr(dd)->numrows = Rf_dpSavedptr(dd)->numrows;
    Rf_dpptr(dd)->numcols = Rf_dpSavedptr(dd)->numcols;
    Rf_dpptr(dd)->currentFigure = Rf_dpSavedptr(dd)->currentFigure;
    Rf_dpptr(dd)->lastFigure = Rf_dpSavedptr(dd)->lastFigure;
    for (i = 0; i < Rf_dpSavedptr(dd)->numrows && i < MAX_LAYOUT_ROWS; i++) {
	Rf_dpptr(dd)->heights[i] = Rf_dpSavedptr(dd)->heights[i];
	Rf_dpptr(dd)->cmHeights[i] = Rf_dpSavedptr(dd)->cmHeights[i];
    }
    for (j = 0; j < Rf_dpSavedptr(dd)->numcols && j < MAX_LAYOUT_COLS; j++) {
	Rf_dpptr(dd)->widths[j] = Rf_dpSavedptr(dd)->widths[j];
	Rf_dpptr(dd)->cmWidths[j] = Rf_dpSavedptr(dd)->cmWidths[j];
    }
    for (i = 0; i < Rf_dpSavedptr(dd)->numrows && i < MAX_LAYOUT_ROWS; i++)
	for (j=0; j<Rf_dpSavedptr(dd)->numcols && j < MAX_LAYOUT_COLS; j++) {
	    Rf_dpptr(dd)->order[i][j] = Rf_dpSavedptr(dd)->order[i][j];
	    Rf_dpptr(dd)->respect[i][j] = Rf_dpSavedptr(dd)->respect[i][j];
	}
    Rf_dpptr(dd)->rspct = Rf_dpSavedptr(dd)->rspct;
    Rf_dpptr(dd)->layout = Rf_dpSavedptr(dd)->layout;
    Rf_dpptr(dd)->mfind = Rf_dpSavedptr(dd)->mfind;
    Rf_dpptr(dd)->new = Rf_dpSavedptr(dd)->new;
    Rf_dpptr(dd)->oma[0] = Rf_dpSavedptr(dd)->oma[0];
    Rf_dpptr(dd)->oma[1] = Rf_dpSavedptr(dd)->oma[1];
    Rf_dpptr(dd)->oma[2] = Rf_dpSavedptr(dd)->oma[2];
    Rf_dpptr(dd)->oma[3] = Rf_dpSavedptr(dd)->oma[3];
    Rf_dpptr(dd)->omi[0] = Rf_dpSavedptr(dd)->omi[0];
    Rf_dpptr(dd)->omi[1] = Rf_dpSavedptr(dd)->omi[1];
    Rf_dpptr(dd)->omi[2] = Rf_dpSavedptr(dd)->omi[2];
    Rf_dpptr(dd)->omi[3] = Rf_dpSavedptr(dd)->omi[3];
    Rf_dpptr(dd)->omd[0] = Rf_dpSavedptr(dd)->omd[0];
    Rf_dpptr(dd)->omd[1] = Rf_dpSavedptr(dd)->omd[1];
    Rf_dpptr(dd)->omd[2] = Rf_dpSavedptr(dd)->omd[2];
    Rf_dpptr(dd)->omd[3] = Rf_dpSavedptr(dd)->omd[3];
    Rf_dpptr(dd)->oUnits = Rf_dpSavedptr(dd)->oUnits;
    Rf_dpptr(dd)->plt[0] = Rf_dpSavedptr(dd)->plt[0];
    Rf_dpptr(dd)->plt[1] = Rf_dpSavedptr(dd)->plt[1];
    Rf_dpptr(dd)->plt[2] = Rf_dpSavedptr(dd)->plt[2];
    Rf_dpptr(dd)->plt[3] = Rf_dpSavedptr(dd)->plt[3];
    Rf_dpptr(dd)->pin[0] = Rf_dpSavedptr(dd)->pin[0];
    Rf_dpptr(dd)->pin[1] = Rf_dpSavedptr(dd)->pin[1];
    Rf_dpptr(dd)->pUnits = Rf_dpSavedptr(dd)->pUnits;
    Rf_dpptr(dd)->defaultPlot = Rf_dpSavedptr(dd)->defaultPlot;
    Rf_dpptr(dd)->pty = Rf_dpSavedptr(dd)->pty;
    Rf_dpptr(dd)->usr[0] = Rf_dpSavedptr(dd)->usr[0];
    Rf_dpptr(dd)->usr[1] = Rf_dpSavedptr(dd)->usr[1];
    Rf_dpptr(dd)->usr[2] = Rf_dpSavedptr(dd)->usr[2];
    Rf_dpptr(dd)->usr[3] = Rf_dpSavedptr(dd)->usr[3];
    Rf_dpptr(dd)->logusr[0] = Rf_dpSavedptr(dd)->logusr[0];
    Rf_dpptr(dd)->logusr[1] = Rf_dpSavedptr(dd)->logusr[1];
    Rf_dpptr(dd)->logusr[2] = Rf_dpSavedptr(dd)->logusr[2];
    Rf_dpptr(dd)->logusr[3] = Rf_dpSavedptr(dd)->logusr[3];
}


/* FIXME : If a non-active window is resized to an invalid size */
/* that window is left active.  */

void playDisplayList(DevDesc *dd)
{
    int savedDevice;
    Rboolean asksave;
    SEXP theList;
    if (dd->newDevStruct) {
	theList = Rf_displayList(dd);
	if (theList != R_NilValue) {
	    asksave = Rf_gpptr(dd)->ask;
	    Rf_gpptr(dd)->ask = TRUE;
	    restoredpSaved(dd);
	    copyGPar(Rf_dpptr(dd), Rf_gpptr(dd));
	    GReset(dd);
	    savedDevice = curDevice();
	    selectDevice(deviceNumber(dd));
	    while (theList != R_NilValue) {
		SEXP theOperation = CAR(theList);
		SEXP op = CAR(theOperation);
		SEXP args = CDR(theOperation);
		PRIMFUN(op) (R_NilValue, op, args, R_NilValue);
		if (!Rf_gpptr(dd)->valid) break;
		theList = CDR(theList);
	    }
	    Rf_gpptr(dd)->ask = asksave;
	    selectDevice(savedDevice);
	}
    }
    else {
	theList = dd->displayList;
	if (theList != R_NilValue) {
	    asksave = Rf_gpptr(dd)->ask;
	    Rf_gpptr(dd)->ask = TRUE;
	    restoredpSaved(dd);
	    copyGPar(Rf_dpptr(dd), Rf_gpptr(dd));
	    GReset(dd);
	    savedDevice = curDevice();
	    selectDevice(deviceNumber(dd));
	    while (theList != R_NilValue) {
		SEXP theOperation = CAR(theList);
		SEXP op = CAR(theOperation);
		SEXP args = CDR(theOperation);
		PRIMFUN(op) (R_NilValue, op, args, R_NilValue);
		if (!Rf_gpptr(dd)->valid) break;
		theList = CDR(theList);
	    }
	    Rf_gpptr(dd)->ask = asksave;
	    selectDevice(savedDevice);
	}
    }
}


/* FIXME:  This assumes that the only drawing is base graphics drawing.
 * For example, copying a display list containing grid drawing will
 * not work properly (grid drawing is not based on a Rf_dpSavedptr;  grid
 * drawing IS based on its own separate graphics state)
 * Once the conversion of device drivers is complete, this should just
 * be able to call GEcopyDisplayList
 */
void copyDisplayList(int fromDevice)
{
    DevDesc *dd = CurrentDevice();
    if (dd->newDevStruct) {
	((GEDevDesc*) dd)->dev->displayList =
	    Rf_displayList(R_Devices[fromDevice]);
	copyGPar(Rf_dpSavedptr(R_Devices[fromDevice]),
		 Rf_dpSavedptr(dd));
	playDisplayList(dd);
	if (!((GEDevDesc*) dd)->dev->displayListOn)
	    initDisplayList(dd);
    } else {
	dd->displayList = R_Devices[fromDevice]->displayList;
	/* Used to be a shallow copy -- most uses just close
	 * the device copied to.
	 *
	 * dd->dpSaved = R_Devices[fromDevice]->dpSaved;
	 *
	 * NOTE that the display list is still only shallow-copied.
	 */
	copyGPar(Rf_dpSavedptr(R_Devices[fromDevice]),
		 Rf_dpSavedptr(dd));
	playDisplayList(dd);
	if (!dd->displayListOn)
	    initDisplayList(dd);
    }
}


void inhibitDisplayList(DevDesc *dd)
{
    GEinitDisplayList((GEDevDesc*) dd);
    if (dd->newDevStruct)
	((GEDevDesc*) dd)->dev->displayListOn = FALSE;
    else
	dd->displayListOn = FALSE;
}

void enableDisplayList(DevDesc *dd)
{
    GEinitDisplayList((GEDevDesc*) dd);
    if (dd->newDevStruct)
	((GEDevDesc*) dd)->dev->displayListOn = TRUE;
    else
	dd->displayListOn = TRUE;
}
