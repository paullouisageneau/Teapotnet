/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of TeapotNet.                                     *
 *                                                                       *
 *   TeapotNet is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   TeapotNet is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with TeapotNet.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#include "tpn/mime.h"

namespace tpn
{

StringMap Mime::Types;
Mutex Mime::TypesMutex;

bool Mime::IsAudio(const String &fileName)
{
  	String extension = fileName.afterLast('.').toLower();
	return  (  extension == "ogg" 
		|| extension == "oga"
		|| extension == "mp3"
		|| extension == "flac"
		|| extension == "wav"
		|| extension == "ape"
		|| extension == "aac"
		|| extension == "m4a"
		|| extension == "mp4a"
		|| extension == "wma");
}

bool Mime::IsVideo(const String &fileName)
{
        String extension = fileName.afterLast('.').toLower();
        return  (  extension == "avi"
                || extension == "mkv"
                || extension == "ogv"
		|| extension == "ogx"
		|| extension == "ogm"
                || extension == "wmv"
                || extension == "asf"
                || extension == "flv"
                || extension == "mpg"
		|| extension == "mpeg"
		|| extension == "mp4"
		|| extension == "m4v"
		|| extension == "mov"
		|| extension == "3gp"
		|| extension == "3g2"
		|| extension == "divx"
		|| extension == "xvid"
		|| extension == "rm"
		|| extension == "rv");
}

String Mime::GetType(const String &fileName)
{
	String extension = fileName.afterLast('.').toLower();
	TypesMutex.lock();
	if(Types.empty()) Init();
	String type;
	if(Types.get(extension, type))
	{
	 	TypesMutex.unlock();
		return type;
	}
	TypesMutex.unlock();
	return "application/octet-stream";
}

void Mime::Init(void)
{
	Types["ez"] = "application/andrew-inset";
	Types["aw"] = "application/applixware";
	Types["atom"] = "application/atom+xml";
	Types["atomcat"] = "application/atomcat+xml";
	Types["atomsvc"] = "application/atomsvc+xml";
	Types["ccxml"] = "application/ccxml+xml";
	Types["cdmia"] = "application/cdmi-capability";
	Types["cdmic"] = "application/cdmi-container";
	Types["cdmid"] = "application/cdmi-domain";
	Types["cdmio"] = "application/cdmi-object";
	Types["cdmiq"] = "application/cdmi-queue";
	Types["cu"] = "application/cu-seeme";
	Types["davmount"] = "application/davmount+xml";
	Types["dbk"] = "application/docbook+xml";
	Types["dssc"] = "application/dssc+der";
	Types["xdssc"] = "application/dssc+xml";
	Types["ecma"] = "application/ecmascript";
	Types["emma"] = "application/emma+xml";
	Types["epub"] = "application/epub+zip";
	Types["exi"] = "application/exi";
	Types["pfr"] = "application/font-tdpfr";
	Types["gml"] = "application/gml+xml";
	Types["gpx"] = "application/gpx+xml";
	Types["gxf"] = "application/gxf";
	Types["stk"] = "application/hyperstudio";
	Types["ink"] = "application/inkml+xml";
	Types["ipfix"] = "application/ipfix";
	Types["jar"] = "application/java-archive";
	Types["ser"] = "application/java-serialized-object";
	Types["class"] = "application/java-vm";
	Types["js"] = "application/javascript";
	Types["json"] = "application/json";
	Types["jsonml"] = "application/jsonml+json";
	Types["lostxml"] = "application/lost+xml";
	Types["hqx"] = "application/mac-binhex40";
	Types["cpt"] = "application/mac-compactpro";
	Types["mads"] = "application/mads+xml";
	Types["mrc"] = "application/marc";
	Types["mrcx"] = "application/marcxml+xml";
	Types["ma"] = "application/mathematica";
	Types["mathml"] = "application/mathml+xml";
	Types["mbox"] = "application/mbox";
	Types["mscml"] = "application/mediaservercontrol+xml";
	Types["metalink"] = "application/metalink+xml";
	Types["meta4"] = "application/metalink4+xml";
	Types["mets"] = "application/mets+xml";
	Types["mods"] = "application/mods+xml";
	Types["m21"] = "application/mp21";
	Types["mp4s"] = "application/mp4";
	Types["doc"] = "application/msword";
	Types["mxf"] = "application/mxf";
	Types["bin"] = "application/octet-stream";
	Types["oda"] = "application/oda";
	Types["opf"] = "application/oebps-package+xml";
	Types["ogx"] = "application/ogg";
	Types["omdoc"] = "application/omdoc+xml";
	Types["onetoc"] = "application/onenote";
	Types["oxps"] = "application/oxps";
	Types["xer"] = "application/patch-ops-error+xml";
	Types["pdf"] = "application/pdf";
	Types["pgp"] = "application/pgp-encrypted";
	Types["asc"] = "application/pgp-signature";
	Types["prf"] = "application/pics-rules";
	Types["p10"] = "application/pkcs10";
	Types["p7m"] = "application/pkcs7-mime";
	Types["p7s"] = "application/pkcs7-signature";
	Types["p8"] = "application/pkcs8";
	Types["ac"] = "application/pkix-attr-cert";
	Types["cer"] = "application/pkix-cert";
	Types["crl"] = "application/pkix-crl";
	Types["pkipath"] = "application/pkix-pkipath";
	Types["pki"] = "application/pkixcmp";
	Types["pls"] = "application/pls+xml";
	Types["ai"] = "application/postscript";
	Types["cww"] = "application/prs.cww";
	Types["pskcxml"] = "application/pskc+xml";
	Types["rdf"] = "application/rdf+xml";
	Types["rif"] = "application/reginfo+xml";
	Types["rnc"] = "application/relax-ng-compact-syntax";
	Types["rl"] = "application/resource-lists+xml";
	Types["rld"] = "application/resource-lists-diff+xml";
	Types["rs"] = "application/rls-services+xml";
	Types["gbr"] = "application/rpki-ghostbusters";
	Types["mft"] = "application/rpki-manifest";
	Types["roa"] = "application/rpki-roa";
	Types["rsd"] = "application/rsd+xml";
	Types["rss"] = "application/rss+xml";
	Types["rtf"] = "application/rtf";
	Types["sbml"] = "application/sbml+xml";
	Types["scq"] = "application/scvp-cv-request";
	Types["scs"] = "application/scvp-cv-response";
	Types["spq"] = "application/scvp-vp-request";
	Types["spp"] = "application/scvp-vp-response";
	Types["sdp"] = "application/sdp";
	Types["setpay"] = "application/set-payment-initiation";
	Types["setreg"] = "application/set-registration-initiation";
	Types["shf"] = "application/shf+xml";
	Types["smi"] = "application/smil+xml";
	Types["rq"] = "application/sparql-query";
	Types["srx"] = "application/sparql-results+xml";
	Types["gram"] = "application/srgs";
	Types["grxml"] = "application/srgs+xml";
	Types["sru"] = "application/sru+xml";
	Types["ssdl"] = "application/ssdl+xml";
	Types["ssml"] = "application/ssml+xml";
	Types["tei"] = "application/tei+xml";
	Types["tfi"] = "application/thraud+xml";
	Types["tsd"] = "application/timestamped-data";
	Types["plb"] = "application/vnd.3gpp.pic-bw-large";
	Types["psb"] = "application/vnd.3gpp.pic-bw-small";
	Types["pvb"] = "application/vnd.3gpp.pic-bw-var";
	Types["tcap"] = "application/vnd.3gpp2.tcap";
	Types["pwn"] = "application/vnd.3m.post-it-notes";
	Types["aso"] = "application/vnd.accpac.simply.aso";
	Types["imp"] = "application/vnd.accpac.simply.imp";
	Types["acu"] = "application/vnd.acucobol";
	Types["atc"] = "application/vnd.acucorp";
	Types["air"] = "application/vnd.adobe.air-application-installer-package+zip";
	Types["fcdt"] = "application/vnd.adobe.formscentral.fcdt";
	Types["fxp"] = "application/vnd.adobe.fxp";
	Types["xdp"] = "application/vnd.adobe.xdp+xml";
	Types["xfdf"] = "application/vnd.adobe.xfdf";
	Types["ahead"] = "application/vnd.ahead.space";
	Types["azf"] = "application/vnd.airzip.filesecure.azf";
	Types["azs"] = "application/vnd.airzip.filesecure.azs";
	Types["azw"] = "application/vnd.amazon.ebook";
	Types["acc"] = "application/vnd.americandynamics.acc";
	Types["ami"] = "application/vnd.amiga.ami";
	Types["apk"] = "application/vnd.android.package-archive";
	Types["cii"] = "application/vnd.anser-web-certificate-issue-initiation";
	Types["fti"] = "application/vnd.anser-web-funds-transfer-initiation";
	Types["atx"] = "application/vnd.antix.game-component";
	Types["mpkg"] = "application/vnd.apple.installer+xml";
	Types["m3u8"] = "application/vnd.apple.mpegurl";
	Types["swi"] = "application/vnd.aristanetworks.swi";
	Types["iota"] = "application/vnd.astraea-software.iota";
	Types["aep"] = "application/vnd.audiograph";
	Types["mpm"] = "application/vnd.blueice.multipass";
	Types["bmi"] = "application/vnd.bmi";
	Types["rep"] = "application/vnd.businessobjects";
	Types["cdxml"] = "application/vnd.chemdraw+xml";
	Types["mmd"] = "application/vnd.chipnuts.karaoke-mmd";
	Types["cdy"] = "application/vnd.cinderella";
	Types["cla"] = "application/vnd.claymore";
	Types["rp9"] = "application/vnd.cloanto.rp9";
	Types["c4g"] = "application/vnd.clonk.c4group";
	Types["c11amc"] = "application/vnd.cluetrust.cartomobile-config";
	Types["c11amz"] = "application/vnd.cluetrust.cartomobile-config-pkg";
	Types["csp"] = "application/vnd.commonspace";
	Types["cdbcmsg"] = "application/vnd.contact.cmsg";
	Types["cmc"] = "application/vnd.cosmocaller";
	Types["clkx"] = "application/vnd.crick.clicker";
	Types["clkk"] = "application/vnd.crick.clicker.keyboard";
	Types["clkp"] = "application/vnd.crick.clicker.palette";
	Types["clkt"] = "application/vnd.crick.clicker.template";
	Types["clkw"] = "application/vnd.crick.clicker.wordbank";
	Types["wbs"] = "application/vnd.criticaltools.wbs+xml";
	Types["pml"] = "application/vnd.ctc-posml";
	Types["ppd"] = "application/vnd.cups-ppd";
	Types["car"] = "application/vnd.curl.car";
	Types["pcurl"] = "application/vnd.curl.pcurl";
	Types["dart"] = "application/vnd.dart";
	Types["rdz"] = "application/vnd.data-vision.rdz";
	Types["uvf"] = "application/vnd.dece.data";
	Types["uvt"] = "application/vnd.dece.ttml+xml";
	Types["uvx"] = "application/vnd.dece.unspecified";
	Types["uvz"] = "application/vnd.dece.zip";
	Types["fe_launch"] = "application/vnd.denovo.fcselayout-link";
	Types["dna"] = "application/vnd.dna";
	Types["mlp"] = "application/vnd.dolby.mlp";
	Types["dpg"] = "application/vnd.dpgraph";
	Types["dfac"] = "application/vnd.dreamfactory";
	Types["kpxx"] = "application/vnd.ds-keypoint";
	Types["ait"] = "application/vnd.dvb.ait";
	Types["svc"] = "application/vnd.dvb.service";
	Types["geo"] = "application/vnd.dynageo";
	Types["mag"] = "application/vnd.ecowin.chart";
	Types["nml"] = "application/vnd.enliven";
	Types["esf"] = "application/vnd.epson.esf";
	Types["msf"] = "application/vnd.epson.msf";
	Types["qam"] = "application/vnd.epson.quickanime";
	Types["slt"] = "application/vnd.epson.salt";
	Types["ssf"] = "application/vnd.epson.ssf";
	Types["es3"] = "application/vnd.eszigno3+xml";
	Types["ez2"] = "application/vnd.ezpix-album";
	Types["ez3"] = "application/vnd.ezpix-package";
	Types["fdf"] = "application/vnd.fdf";
	Types["mseed"] = "application/vnd.fdsn.mseed";
	Types["seed"] = "application/vnd.fdsn.seed";
	Types["gph"] = "application/vnd.flographit";
	Types["ftc"] = "application/vnd.fluxtime.clip";
	Types["fm"] = "application/vnd.framemaker";
	Types["fnc"] = "application/vnd.frogans.fnc";
	Types["ltf"] = "application/vnd.frogans.ltf";
	Types["fsc"] = "application/vnd.fsc.weblaunch";
	Types["oas"] = "application/vnd.fujitsu.oasys";
	Types["oa2"] = "application/vnd.fujitsu.oasys2";
	Types["oa3"] = "application/vnd.fujitsu.oasys3";
	Types["fg5"] = "application/vnd.fujitsu.oasysgp";
	Types["bh2"] = "application/vnd.fujitsu.oasysprs";
	Types["ddd"] = "application/vnd.fujixerox.ddd";
	Types["xdw"] = "application/vnd.fujixerox.docuworks";
	Types["xbd"] = "application/vnd.fujixerox.docuworks.binder";
	Types["fzs"] = "application/vnd.fuzzysheet";
	Types["txd"] = "application/vnd.genomatix.tuxedo";
	Types["ggb"] = "application/vnd.geogebra.file";
	Types["ggt"] = "application/vnd.geogebra.tool";
	Types["gex"] = "application/vnd.geometry-explorer";
	Types["gxt"] = "application/vnd.geonext";
	Types["g2w"] = "application/vnd.geoplan";
	Types["g3w"] = "application/vnd.geospace";
	Types["gmx"] = "application/vnd.gmx";
	Types["kml"] = "application/vnd.google-earth.kml+xml";
	Types["kmz"] = "application/vnd.google-earth.kmz";
	Types["gqf"] = "application/vnd.grafeq";
	Types["gac"] = "application/vnd.groove-account";
	Types["ghf"] = "application/vnd.groove-help";
	Types["gim"] = "application/vnd.groove-identity-message";
	Types["grv"] = "application/vnd.groove-injector";
	Types["gtm"] = "application/vnd.groove-tool-message";
	Types["tpl"] = "application/vnd.groove-tool-template";
	Types["vcg"] = "application/vnd.groove-vcard";
	Types["hal"] = "application/vnd.hal+xml";
	Types["zmm"] = "application/vnd.handheld-entertainment+xml";
	Types["hbci"] = "application/vnd.hbci";
	Types["les"] = "application/vnd.hhe.lesson-player";
	Types["hpgl"] = "application/vnd.hp-hpgl";
	Types["hpid"] = "application/vnd.hp-hpid";
	Types["hps"] = "application/vnd.hp-hps";
	Types["jlt"] = "application/vnd.hp-jlyt";
	Types["pcl"] = "application/vnd.hp-pcl";
	Types["pclxl"] = "application/vnd.hp-pclxl";
	Types["sfd-hdstx"] = "application/vnd.hydrostatix.sof-data";
	Types["mpy"] = "application/vnd.ibm.minipay";
	Types["afp"] = "application/vnd.ibm.modcap";
	Types["irm"] = "application/vnd.ibm.rights-management";
	Types["sc"] = "application/vnd.ibm.secure-container";
	Types["icc"] = "application/vnd.iccprofile";
	Types["igl"] = "application/vnd.igloader";
	Types["ivp"] = "application/vnd.immervision-ivp";
	Types["ivu"] = "application/vnd.immervision-ivu";
	Types["igm"] = "application/vnd.insors.igm";
	Types["xpw"] = "application/vnd.intercon.formnet";
	Types["i2g"] = "application/vnd.intergeo";
	Types["qbo"] = "application/vnd.intu.qbo";
	Types["qfx"] = "application/vnd.intu.qfx";
	Types["rcprofile"] = "application/vnd.ipunplugged.rcprofile";
	Types["irp"] = "application/vnd.irepository.package+xml";
	Types["xpr"] = "application/vnd.is-xpr";
	Types["fcs"] = "application/vnd.isac.fcs";
	Types["jam"] = "application/vnd.jam";
	Types["rms"] = "application/vnd.jcp.javame.midlet-rms";
	Types["jisp"] = "application/vnd.jisp";
	Types["joda"] = "application/vnd.joost.joda-archive";
	Types["ktz"] = "application/vnd.kahootz";
	Types["karbon"] = "application/vnd.kde.karbon";
	Types["chrt"] = "application/vnd.kde.kchart";
	Types["kfo"] = "application/vnd.kde.kformula";
	Types["flw"] = "application/vnd.kde.kivio";
	Types["kon"] = "application/vnd.kde.kontour";
	Types["kpr"] = "application/vnd.kde.kpresenter";
	Types["ksp"] = "application/vnd.kde.kspread";
	Types["kwd"] = "application/vnd.kde.kword";
	Types["htke"] = "application/vnd.kenameaapp";
	Types["kia"] = "application/vnd.kidspiration";
	Types["kne"] = "application/vnd.kinar";
	Types["skp"] = "application/vnd.koan";
	Types["sse"] = "application/vnd.kodak-descriptor";
	Types["lasxml"] = "application/vnd.las.las+xml";
	Types["lbd"] = "application/vnd.llamagraphics.life-balance.desktop";
	Types["lbe"] = "application/vnd.llamagraphics.life-balance.exchange+xml";
	Types["123"] = "application/vnd.lotus-1-2-3";
	Types["apr"] = "application/vnd.lotus-approach";
	Types["pre"] = "application/vnd.lotus-freelance";
	Types["nsf"] = "application/vnd.lotus-notes";
	Types["org"] = "application/vnd.lotus-organizer";
	Types["scm"] = "application/vnd.lotus-screencam";
	Types["lwp"] = "application/vnd.lotus-wordpro";
	Types["portpkg"] = "application/vnd.macports.portpkg";
	Types["mcd"] = "application/vnd.mcd";
	Types["mc1"] = "application/vnd.medcalcdata";
	Types["cdkey"] = "application/vnd.mediastation.cdkey";
	Types["mwf"] = "application/vnd.mfer";
	Types["mfm"] = "application/vnd.mfmp";
	Types["flo"] = "application/vnd.micrografx.flo";
	Types["igx"] = "application/vnd.micrografx.igx";
	Types["mif"] = "application/vnd.mif";
	Types["daf"] = "application/vnd.mobius.daf";
	Types["dis"] = "application/vnd.mobius.dis";
	Types["mbk"] = "application/vnd.mobius.mbk";
	Types["mqy"] = "application/vnd.mobius.mqy";
	Types["msl"] = "application/vnd.mobius.msl";
	Types["plc"] = "application/vnd.mobius.plc";
	Types["txf"] = "application/vnd.mobius.txf";
	Types["mpn"] = "application/vnd.mophun.application";
	Types["mpc"] = "application/vnd.mophun.certificate";
	Types["xul"] = "application/vnd.mozilla.xul+xml";
	Types["cil"] = "application/vnd.ms-artgalry";
	Types["cab"] = "application/vnd.ms-cab-compressed";
	Types["xls"] = "application/vnd.ms-excel";
	Types["xlam"] = "application/vnd.ms-excel.addin.macroenabled.12";
	Types["xlsb"] = "application/vnd.ms-excel.sheet.binary.macroenabled.12";
	Types["xlsm"] = "application/vnd.ms-excel.sheet.macroenabled.12";
	Types["xltm"] = "application/vnd.ms-excel.template.macroenabled.12";
	Types["eot"] = "application/vnd.ms-fontobject";
	Types["chm"] = "application/vnd.ms-htmlhelp";
	Types["ims"] = "application/vnd.ms-ims";
	Types["lrm"] = "application/vnd.ms-lrm";
	Types["thmx"] = "application/vnd.ms-officetheme";
	Types["cat"] = "application/vnd.ms-pki.seccat";
	Types["stl"] = "application/vnd.ms-pki.stl";
	Types["ppt"] = "application/vnd.ms-powerpoint";
	Types["ppam"] = "application/vnd.ms-powerpoint.addin.macroenabled.12";
	Types["pptm"] = "application/vnd.ms-powerpoint.presentation.macroenabled.12";
	Types["sldm"] = "application/vnd.ms-powerpoint.slide.macroenabled.12";
	Types["ppsm"] = "application/vnd.ms-powerpoint.slideshow.macroenabled.12";
	Types["potm"] = "application/vnd.ms-powerpoint.template.macroenabled.12";
	Types["mpp"] = "application/vnd.ms-project";
	Types["docm"] = "application/vnd.ms-word.document.macroenabled.12";
	Types["dotm"] = "application/vnd.ms-word.template.macroenabled.12";
	Types["wps"] = "application/vnd.ms-works";
	Types["wpl"] = "application/vnd.ms-wpl";
	Types["xps"] = "application/vnd.ms-xpsdocument";
	Types["mseq"] = "application/vnd.mseq";
	Types["mus"] = "application/vnd.musician";
	Types["msty"] = "application/vnd.muvee.style";
	Types["taglet"] = "application/vnd.mynfc";
	Types["nlu"] = "application/vnd.neurolanguage.nlu";
	Types["ntf"] = "application/vnd.nitf";
	Types["nnd"] = "application/vnd.noblenet-directory";
	Types["nns"] = "application/vnd.noblenet-sealer";
	Types["nnw"] = "application/vnd.noblenet-web";
	Types["ngdat"] = "application/vnd.nokia.n-gage.data";
	Types["n-gage"] = "application/vnd.nokia.n-gage.symbian.install";
	Types["rpst"] = "application/vnd.nokia.radio-preset";
	Types["rpss"] = "application/vnd.nokia.radio-presets";
	Types["edm"] = "application/vnd.novadigm.edm";
	Types["edx"] = "application/vnd.novadigm.edx";
	Types["ext"] = "application/vnd.novadigm.ext";
	Types["odc"] = "application/vnd.oasis.opendocument.chart";
	Types["otc"] = "application/vnd.oasis.opendocument.chart-template";
	Types["odb"] = "application/vnd.oasis.opendocument.database";
	Types["odf"] = "application/vnd.oasis.opendocument.formula";
	Types["odft"] = "application/vnd.oasis.opendocument.formula-template";
	Types["odg"] = "application/vnd.oasis.opendocument.graphics";
	Types["otg"] = "application/vnd.oasis.opendocument.graphics-template";
	Types["odi"] = "application/vnd.oasis.opendocument.image";
	Types["oti"] = "application/vnd.oasis.opendocument.image-template";
	Types["odp"] = "application/vnd.oasis.opendocument.presentation";
	Types["otp"] = "application/vnd.oasis.opendocument.presentation-template";
	Types["ods"] = "application/vnd.oasis.opendocument.spreadsheet";
	Types["ots"] = "application/vnd.oasis.opendocument.spreadsheet-template";
	Types["odt"] = "application/vnd.oasis.opendocument.text";
	Types["odm"] = "application/vnd.oasis.opendocument.text-master";
	Types["ott"] = "application/vnd.oasis.opendocument.text-template";
	Types["oth"] = "application/vnd.oasis.opendocument.text-web";
	Types["xo"] = "application/vnd.olpc-sugar";
	Types["dd2"] = "application/vnd.oma.dd2+xml";
	Types["oxt"] = "application/vnd.openofficeorg.extension";
	Types["pptx"] = "application/vnd.openxmlformats-officedocument.presentationml.presentation";
	Types["sldx"] = "application/vnd.openxmlformats-officedocument.presentationml.slide";
	Types["ppsx"] = "application/vnd.openxmlformats-officedocument.presentationml.slideshow";
	Types["potx"] = "application/vnd.openxmlformats-officedocument.presentationml.template";
	Types["xlsx"] = "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
	Types["xltx"] = "application/vnd.openxmlformats-officedocument.spreadsheetml.template";
	Types["docx"] = "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
	Types["dotx"] = "application/vnd.openxmlformats-officedocument.wordprocessingml.template";
	Types["mgp"] = "application/vnd.osgeo.mapguide.package";
	Types["dp"] = "application/vnd.osgi.dp";
	Types["esa"] = "application/vnd.osgi.subsystem";
	Types["pdb"] = "application/vnd.palm";
	Types["paw"] = "application/vnd.pawaafile";
	Types["str"] = "application/vnd.pg.format";
	Types["ei6"] = "application/vnd.pg.osasli";
	Types["efif"] = "application/vnd.picsel";
	Types["wg"] = "application/vnd.pmi.widget";
	Types["plf"] = "application/vnd.pocketlearn";
	Types["pbd"] = "application/vnd.powerbuilder6";
	Types["box"] = "application/vnd.previewsystems.box";
	Types["mgz"] = "application/vnd.proteus.magazine";
	Types["qps"] = "application/vnd.publishare-delta-tree";
	Types["ptid"] = "application/vnd.pvi.ptid1";
	Types["qxd"] = "application/vnd.quark.quarkxpress";
	Types["bed"] = "application/vnd.realvnc.bed";
	Types["mxl"] = "application/vnd.recordare.musicxml";
	Types["musicxml"] = "application/vnd.recordare.musicxml+xml";
	Types["cryptonote"] = "application/vnd.rig.cryptonote";
	Types["cod"] = "application/vnd.rim.cod";
	Types["rm"] = "application/vnd.rn-realmedia";
	Types["rmvb"] = "application/vnd.rn-realmedia-vbr";
	Types["link66"] = "application/vnd.route66.link66+xml";
	Types["st"] = "application/vnd.sailingtracker.track";
	Types["see"] = "application/vnd.seemail";
	Types["sema"] = "application/vnd.sema";
	Types["semd"] = "application/vnd.semd";
	Types["semf"] = "application/vnd.semf";
	Types["ifm"] = "application/vnd.shana.informed.formdata";
	Types["itp"] = "application/vnd.shana.informed.formtemplate";
	Types["iif"] = "application/vnd.shana.informed.interchange";
	Types["ipk"] = "application/vnd.shana.informed.package";
	Types["twd"] = "application/vnd.simtech-mindmapper";
	Types["mmf"] = "application/vnd.smaf";
	Types["teacher"] = "application/vnd.smart.teacher";
	Types["sdkm"] = "application/vnd.solent.sdkm+xml";
	Types["dxp"] = "application/vnd.spotfire.dxp";
	Types["sfs"] = "application/vnd.spotfire.sfs";
	Types["sdc"] = "application/vnd.stardivision.calc";
	Types["sda"] = "application/vnd.stardivision.draw";
	Types["sdd"] = "application/vnd.stardivision.impress";
	Types["smf"] = "application/vnd.stardivision.math";
	Types["sdw"] = "application/vnd.stardivision.writer";
	Types["sgl"] = "application/vnd.stardivision.writer-global";
	Types["smzip"] = "application/vnd.stepmania.package";
	Types["sm"] = "application/vnd.stepmania.stepchart";
	Types["sxc"] = "application/vnd.sun.xml.calc";
	Types["stc"] = "application/vnd.sun.xml.calc.template";
	Types["sxd"] = "application/vnd.sun.xml.draw";
	Types["std"] = "application/vnd.sun.xml.draw.template";
	Types["sxi"] = "application/vnd.sun.xml.impress";
	Types["sti"] = "application/vnd.sun.xml.impress.template";
	Types["sxm"] = "application/vnd.sun.xml.math";
	Types["sxw"] = "application/vnd.sun.xml.writer";
	Types["sxg"] = "application/vnd.sun.xml.writer.global";
	Types["stw"] = "application/vnd.sun.xml.writer.template";
	Types["sus"] = "application/vnd.sus-calendar";
	Types["svd"] = "application/vnd.svd";
	Types["sis"] = "application/vnd.symbian.install";
	Types["xsm"] = "application/vnd.syncml+xml";
	Types["bdm"] = "application/vnd.syncml.dm+wbxml";
	Types["xdm"] = "application/vnd.syncml.dm+xml";
	Types["tao"] = "application/vnd.tao.intent-module-archive";
	Types["pcap"] = "application/vnd.tcpdump.pcap";
	Types["tmo"] = "application/vnd.tmobile-livetv";
	Types["tpt"] = "application/vnd.trid.tpt";
	Types["mxs"] = "application/vnd.triscape.mxs";
	Types["tra"] = "application/vnd.trueapp";
	Types["ufd"] = "application/vnd.ufdl";
	Types["utz"] = "application/vnd.uiq.theme";
	Types["umj"] = "application/vnd.umajin";
	Types["unityweb"] = "application/vnd.unity";
	Types["uoml"] = "application/vnd.uoml+xml";
	Types["vcx"] = "application/vnd.vcx";
	Types["vsd"] = "application/vnd.visio";
	Types["vis"] = "application/vnd.visionary";
	Types["vsf"] = "application/vnd.vsf";
	Types["wbxml"] = "application/vnd.wap.wbxml";
	Types["wmlc"] = "application/vnd.wap.wmlc";
	Types["wmlsc"] = "application/vnd.wap.wmlscriptc";
	Types["wtb"] = "application/vnd.webturbo";
	Types["nbp"] = "application/vnd.wolfram.player";
	Types["wpd"] = "application/vnd.wordperfect";
	Types["wqd"] = "application/vnd.wqd";
	Types["stf"] = "application/vnd.wt.stf";
	Types["xar"] = "application/vnd.xara";
	Types["xfdl"] = "application/vnd.xfdl";
	Types["hvd"] = "application/vnd.yamaha.hv-dic";
	Types["hvs"] = "application/vnd.yamaha.hv-script";
	Types["hvp"] = "application/vnd.yamaha.hv-voice";
	Types["osf"] = "application/vnd.yamaha.openscoreformat";
	Types["osfpvg"] = "application/vnd.yamaha.openscoreformat.osfpvg+xml";
	Types["saf"] = "application/vnd.yamaha.smaf-audio";
	Types["spf"] = "application/vnd.yamaha.smaf-phrase";
	Types["cmp"] = "application/vnd.yellowriver-custom-menu";
	Types["zir"] = "application/vnd.zul";
	Types["zaz"] = "application/vnd.zzazz.deck+xml";
	Types["vxml"] = "application/voicexml+xml";
	Types["wgt"] = "application/widget";
	Types["hlp"] = "application/winhlp";
	Types["wsdl"] = "application/wsdl+xml";
	Types["wspolicy"] = "application/wspolicy+xml";
	Types["7z"] = "application/x-7z-compressed";
	Types["abw"] = "application/x-abiword";
	Types["ace"] = "application/x-ace-compressed";
	Types["dmg"] = "application/x-apple-diskimage";
	Types["aab"] = "application/x-authorware-bin";
	Types["aam"] = "application/x-authorware-map";
	Types["aas"] = "application/x-authorware-seg";
	Types["bcpio"] = "application/x-bcpio";
	Types["torrent"] = "application/x-bittorrent";
	Types["blb"] = "application/x-blorb";
	Types["bz"] = "application/x-bzip";
	Types["bz2"] = "application/x-bzip2";
	Types["cbr"] = "application/x-cbr";
	Types["vcd"] = "application/x-cdlink";
	Types["cfs"] = "application/x-cfs-compressed";
	Types["chat"] = "application/x-chat";
	Types["pgn"] = "application/x-chess-pgn";
	Types["nsc"] = "application/x-conference";
	Types["cpio"] = "application/x-cpio";
	Types["csh"] = "application/x-csh";
	Types["deb"] = "application/x-debian-package";
	Types["dgc"] = "application/x-dgc-compressed";
	Types["dir"] = "application/x-director";
	Types["wad"] = "application/x-doom";
	Types["ncx"] = "application/x-dtbncx+xml";
	Types["dtb"] = "application/x-dtbook+xml";
	Types["res"] = "application/x-dtbresource+xml";
	Types["dvi"] = "application/x-dvi";
	Types["evy"] = "application/x-envoy";
	Types["eva"] = "application/x-eva";
	Types["bdf"] = "application/x-font-bdf";
	Types["gsf"] = "application/x-font-ghostscript";
	Types["psf"] = "application/x-font-linux-psf";
	Types["otf"] = "application/x-font-otf";
	Types["pcf"] = "application/x-font-pcf";
	Types["snf"] = "application/x-font-snf";
	Types["ttf"] = "application/x-font-ttf";
	Types["pfa"] = "application/x-font-type1";
	Types["woff"] = "application/x-font-woff";
	Types["arc"] = "application/x-freearc";
	Types["spl"] = "application/x-futuresplash";
	Types["gca"] = "application/x-gca-compressed";
	Types["ulx"] = "application/x-glulx";
	Types["gnumeric"] = "application/x-gnumeric";
	Types["gramps"] = "application/x-gramps-xml";
	Types["gtar"] = "application/x-gtar";
	Types["hdf"] = "application/x-hdf";
	Types["install"] = "application/x-install-instructions";
	Types["iso"] = "application/x-iso9660-image";
	Types["jnlp"] = "application/x-java-jnlp-file";
	Types["latex"] = "application/x-latex";
	Types["lzh"] = "application/x-lzh-compressed";
	Types["mie"] = "application/x-mie";
	Types["prc"] = "application/x-mobipocket-ebook";
	Types["application"] = "application/x-ms-application";
	Types["lnk"] = "application/x-ms-shortcut";
	Types["wmd"] = "application/x-ms-wmd";
	Types["wmz"] = "application/x-ms-wmz";
	Types["xbap"] = "application/x-ms-xbap";
	Types["mdb"] = "application/x-msaccess";
	Types["obd"] = "application/x-msbinder";
	Types["crd"] = "application/x-mscardfile";
	Types["clp"] = "application/x-msclip";
	Types["exe"] = "application/x-msdownload";
	Types["mvb"] = "application/x-msmediaview";
	Types["wmf"] = "application/x-msmetafile";
	Types["mny"] = "application/x-msmoney";
	Types["pub"] = "application/x-mspublisher";
	Types["scd"] = "application/x-msschedule";
	Types["trm"] = "application/x-msterminal";
	Types["wri"] = "application/x-mswrite";
	Types["nc"] = "application/x-netcdf";
	Types["nzb"] = "application/x-nzb";
	Types["p12"] = "application/x-pkcs12";
	Types["p7b"] = "application/x-pkcs7-certificates";
	Types["p7r"] = "application/x-pkcs7-certreqresp";
	Types["rar"] = "application/x-rar-compressed";
	Types["ris"] = "application/x-research-info-systems";
	Types["sh"] = "application/x-sh";
	Types["shar"] = "application/x-shar";
	Types["swf"] = "application/x-shockwave-flash";
	Types["xap"] = "application/x-silverlight-app";
	Types["sql"] = "application/x-sql";
	Types["sit"] = "application/x-stuffit";
	Types["sitx"] = "application/x-stuffitx";
	Types["srt"] = "application/x-subrip";
	Types["sv4cpio"] = "application/x-sv4cpio";
	Types["sv4crc"] = "application/x-sv4crc";
	Types["t3"] = "application/x-t3vm-image";
	Types["gam"] = "application/x-tads";
	Types["tar"] = "application/x-tar";
	Types["tcl"] = "application/x-tcl";
	Types["tex"] = "application/x-tex";
	Types["tfm"] = "application/x-tex-tfm";
	Types["texinfo"] = "application/x-texinfo";
	Types["obj"] = "application/x-tgif";
	Types["ustar"] = "application/x-ustar";
	Types["src"] = "application/x-wais-source";
	Types["der"] = "application/x-x509-ca-cert";
	Types["fig"] = "application/x-xfig";
	Types["xlf"] = "application/x-xliff+xml";
	Types["xpi"] = "application/x-xpinstall";
	Types["xz"] = "application/x-xz";
	Types["z1"] = "application/x-zmachine";
	Types["xaml"] = "application/xaml+xml";
	Types["xdf"] = "application/xcap-diff+xml";
	Types["xenc"] = "application/xenc+xml";
	Types["xhtml"] = "application/xhtml+xml";
	Types["xml"] = "application/xml";
	Types["dtd"] = "application/xml-dtd";
	Types["xop"] = "application/xop+xml";
	Types["xpl"] = "application/xproc+xml";
	Types["xslt"] = "application/xslt+xml";
	Types["xspf"] = "application/xspf+xml";
	Types["mxml"] = "application/xv+xml";
	Types["yang"] = "application/yang";
	Types["yin"] = "application/yin+xml";
	Types["zip"] = "application/zip";
	Types["adp"] = "audio/adpcm";
	Types["au"] = "audio/basic";
	Types["mid"] = "audio/midi";
	Types["mp4a"] = "audio/mp4";
	Types["m4a"] = "audio/mp4";
	Types["mpga"] = "audio/mpeg";
	Types["oga"] = "audio/ogg";
	Types["ogg"] = "audio/ogg";
	Types["s3m"] = "audio/s3m";
	Types["sil"] = "audio/silk";
	Types["uva"] = "audio/vnd.dece.audio";
	Types["eol"] = "audio/vnd.digital-winds";
	Types["dra"] = "audio/vnd.dra";
	Types["dts"] = "audio/vnd.dts";
	Types["dtshd"] = "audio/vnd.dts.hd";
	Types["lvp"] = "audio/vnd.lucent.voice";
	Types["pya"] = "audio/vnd.ms-playready.media.pya";
	Types["ecelp4800"] = "audio/vnd.nuera.ecelp4800";
	Types["ecelp7470"] = "audio/vnd.nuera.ecelp7470";
	Types["ecelp9600"] = "audio/vnd.nuera.ecelp9600";
	Types["rip"] = "audio/vnd.rip";
	Types["weba"] = "audio/webm";
	Types["aac"] = "audio/x-aac";
	Types["aif"] = "audio/x-aiff";
	Types["caf"] = "audio/x-caf";
	Types["flac"] = "audio/x-flac";
	Types["mka"] = "audio/x-matroska";
	Types["m3u"] = "audio/x-mpegurl";
	Types["mp3"] = "audio/mpeg";
	Types["wax"] = "audio/x-ms-wax";
	Types["wma"] = "audio/x-ms-wma";
	Types["ram"] = "audio/x-pn-realaudio";
	Types["rmp"] = "audio/x-pn-realaudio-plugin";
	Types["wav"] = "audio/x-wav";
	Types["xm"] = "audio/xm";
	Types["cdx"] = "chemical/x-cdx";
	Types["cif"] = "chemical/x-cif";
	Types["cmdf"] = "chemical/x-cmdf";
	Types["cml"] = "chemical/x-cml";
	Types["csml"] = "chemical/x-csml";
	Types["xyz"] = "chemical/x-xyz";
	Types["bmp"] = "image/bmp";
	Types["cgm"] = "image/cgm";
	Types["g3"] = "image/g3fax";
	Types["gif"] = "image/gif";
	Types["ief"] = "image/ief";
	Types["jpeg"] = "image/jpeg";
	Types["jpg"] = "image/jpeg";
	Types["ktx"] = "image/ktx";
	Types["png"] = "image/png";
	Types["btif"] = "image/prs.btif";
	Types["sgi"] = "image/sgi";
	Types["svg"] = "image/svg+xml";
	Types["tiff"] = "image/tiff";
	Types["psd"] = "image/vnd.adobe.photoshop";
	Types["uvi"] = "image/vnd.dece.graphic";
	Types["sub"] = "image/vnd.dvb.subtitle";
	Types["djvu"] = "image/vnd.djvu";
	Types["dwg"] = "image/vnd.dwg";
	Types["dxf"] = "image/vnd.dxf";
	Types["fbs"] = "image/vnd.fastbidsheet";
	Types["fpx"] = "image/vnd.fpx";
	Types["fst"] = "image/vnd.fst";
	Types["mmr"] = "image/vnd.fujixerox.edmics-mmr";
	Types["rlc"] = "image/vnd.fujixerox.edmics-rlc";
	Types["mdi"] = "image/vnd.ms-modi";
	Types["wdp"] = "image/vnd.ms-photo";
	Types["npx"] = "image/vnd.net-fpx";
	Types["wbmp"] = "image/vnd.wap.wbmp";
	Types["xif"] = "image/vnd.xiff";
	Types["webp"] = "image/webp";
	Types["3ds"] = "image/x-3ds";
	Types["ras"] = "image/x-cmu-raster";
	Types["cmx"] = "image/x-cmx";
	Types["fh"] = "image/x-freehand";
	Types["ico"] = "image/x-icon";
	Types["sid"] = "image/x-mrsid-image";
	Types["pcx"] = "image/x-pcx";
	Types["pic"] = "image/x-pict";
	Types["pnm"] = "image/x-portable-anymap";
	Types["pbm"] = "image/x-portable-bitmap";
	Types["pgm"] = "image/x-portable-graymap";
	Types["ppm"] = "image/x-portable-pixmap";
	Types["rgb"] = "image/x-rgb";
	Types["tga"] = "image/x-tga";
	Types["xbm"] = "image/x-xbitmap";
	Types["xpm"] = "image/x-xpixmap";
	Types["xwd"] = "image/x-xwindowdump";
	Types["eml"] = "message/rfc822";
	Types["igs"] = "model/iges";
	Types["msh"] = "model/mesh";
	Types["dae"] = "model/vnd.collada+xml";
	Types["dwf"] = "model/vnd.dwf";
	Types["gdl"] = "model/vnd.gdl";
	Types["gtw"] = "model/vnd.gtw";
	Types["mts"] = "model/vnd.mts";
	Types["vtu"] = "model/vnd.vtu";
	Types["wrl"] = "model/vrml";
	Types["x3db"] = "model/x3d+binary";
	Types["x3dv"] = "model/x3d+vrml";
	Types["x3d"] = "model/x3d+xml";
	Types["appcache"] = "text/cache-manifest";
	Types["ics"] = "text/calendar";
	Types["css"] = "text/css";
	Types["csv"] = "text/csv";
	Types["html"] = "text/html";
	Types["n3"] = "text/n3";
	Types["txt"] = "text/plain";
	Types["dsc"] = "text/prs.lines.tag";
	Types["rtx"] = "text/richtext";
	Types["sgml"] = "text/sgml";
	Types["tsv"] = "text/tab-separated-values";
	Types["t"] = "text/troff";
	Types["ttl"] = "text/turtle";
	Types["uri"] = "text/uri-list";
	Types["vcard"] = "text/vcard";
	Types["curl"] = "text/vnd.curl";
	Types["dcurl"] = "text/vnd.curl.dcurl";
	Types["scurl"] = "text/vnd.curl.scurl";
	Types["mcurl"] = "text/vnd.curl.mcurl";
	Types["sub"] = "text/vnd.dvb.subtitle";
	Types["fly"] = "text/vnd.fly";
	Types["flx"] = "text/vnd.fmi.flexstor";
	Types["gv"] = "text/vnd.graphviz";
	Types["3dml"] = "text/vnd.in3d.3dml";
	Types["spot"] = "text/vnd.in3d.spot";
	Types["jad"] = "text/vnd.sun.j2me.app-descriptor";
	Types["wml"] = "text/vnd.wap.wml";
	Types["wmls"] = "text/vnd.wap.wmlscript";
	Types["s"] = "text/x-asm";
	Types["c"] = "text/x-c";
	Types["f"] = "text/x-fortran";
	Types["java"] = "text/x-java-source";
	Types["opml"] = "text/x-opml";
	Types["p"] = "text/x-pascal";
	Types["nfo"] = "text/x-nfo";
	Types["etx"] = "text/x-setext";
	Types["sfv"] = "text/x-sfv";
	Types["uu"] = "text/x-uuencode";
	Types["vcs"] = "text/x-vcalendar";
	Types["vcf"] = "text/x-vcard";
	Types["3gp"] = "video/3gpp";
	Types["3g2"] = "video/3gpp2";
	Types["h261"] = "video/h261";
	Types["h263"] = "video/h263";
	Types["h264"] = "video/h264";
	Types["jpgv"] = "video/jpeg";
	Types["jpm"] = "video/jpm";
	Types["mj2"] = "video/mj2";
	Types["mp4"] = "video/mp4";
	Types["m4v"] = "video/mp4";
	Types["mpeg"] = "video/mpeg";
	Types["ogv"] = "video/ogg";
	Types["ogm"] = "video/ogg";
	Types["qt"] = "video/quicktime";
	Types["uvh"] = "video/vnd.dece.hd";
	Types["uvm"] = "video/vnd.dece.mobile";
	Types["uvp"] = "video/vnd.dece.pd";
	Types["uvs"] = "video/vnd.dece.sd";
	Types["uvv"] = "video/vnd.dece.video";
	Types["dvb"] = "video/vnd.dvb.file";
	Types["fvt"] = "video/vnd.fvt";
	Types["mxu"] = "video/vnd.mpegurl";
	Types["pyv"] = "video/vnd.ms-playready.media.pyv";
	Types["uvu"] = "video/vnd.uvvu.mp4";
	Types["viv"] = "video/vnd.vivo";
	Types["webm"] = "video/webm";
	Types["f4v"] = "video/x-f4v";
	Types["fli"] = "video/x-fli";
	Types["flv"] = "video/x-flv";
	Types["m4v"] = "video/x-m4v";
	Types["mkv"] = "video/x-matroska";
	Types["mng"] = "video/x-mng";
	Types["asf"] = "video/x-ms-asf";
	Types["vob"] = "video/x-ms-vob";
	Types["wm"] = "video/x-ms-wm";
	Types["wmv"] = "video/x-ms-wmv";
	Types["wmx"] = "video/x-ms-wmx";
	Types["wvx"] = "video/x-ms-wvx";
	Types["avi"] = "video/x-msvideo";
	Types["movie"] = "video/x-sgi-movie";
	Types["smv"] = "video/x-smv";
	Types["ice"] = "x-conference/x-cooltalk";  
}

}
