<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE eagle SYSTEM "eagle.dtd">
<eagle version="9.3.1">
<drawing>
<settings>
<setting alwaysvectorfont="no"/>
<setting verticaltext="up"/>
</settings>
<grid distance="0.1" unitdist="inch" unit="inch" style="lines" multiple="1" display="no" altdistance="0.01" altunitdist="inch" altunit="inch"/>
<layers>
<layer number="1" name="Top" color="4" fill="1" visible="no" active="no"/>
<layer number="2" name="Route2" color="1" fill="3" visible="no" active="no"/>
<layer number="15" name="Route15" color="4" fill="6" visible="no" active="no"/>
<layer number="16" name="Bottom" color="1" fill="1" visible="no" active="no"/>
<layer number="17" name="Pads" color="2" fill="1" visible="no" active="no"/>
<layer number="18" name="Vias" color="2" fill="1" visible="no" active="no"/>
<layer number="19" name="Unrouted" color="6" fill="1" visible="no" active="no"/>
<layer number="20" name="Dimension" color="15" fill="1" visible="no" active="no"/>
<layer number="21" name="tPlace" color="7" fill="1" visible="no" active="no"/>
<layer number="22" name="bPlace" color="7" fill="1" visible="no" active="no"/>
<layer number="23" name="tOrigins" color="15" fill="1" visible="no" active="no"/>
<layer number="24" name="bOrigins" color="15" fill="1" visible="no" active="no"/>
<layer number="25" name="tNames" color="7" fill="1" visible="no" active="no"/>
<layer number="26" name="bNames" color="7" fill="1" visible="no" active="no"/>
<layer number="27" name="tValues" color="7" fill="1" visible="no" active="no"/>
<layer number="28" name="bValues" color="7" fill="1" visible="no" active="no"/>
<layer number="29" name="tStop" color="7" fill="3" visible="no" active="no"/>
<layer number="30" name="bStop" color="7" fill="6" visible="no" active="no"/>
<layer number="31" name="tCream" color="7" fill="4" visible="no" active="no"/>
<layer number="32" name="bCream" color="7" fill="5" visible="no" active="no"/>
<layer number="33" name="tFinish" color="6" fill="3" visible="no" active="no"/>
<layer number="34" name="bFinish" color="6" fill="6" visible="no" active="no"/>
<layer number="35" name="tGlue" color="7" fill="4" visible="no" active="no"/>
<layer number="36" name="bGlue" color="7" fill="5" visible="no" active="no"/>
<layer number="37" name="tTest" color="7" fill="1" visible="no" active="no"/>
<layer number="38" name="bTest" color="7" fill="1" visible="no" active="no"/>
<layer number="39" name="tKeepout" color="4" fill="11" visible="no" active="no"/>
<layer number="40" name="bKeepout" color="1" fill="11" visible="no" active="no"/>
<layer number="41" name="tRestrict" color="4" fill="10" visible="no" active="no"/>
<layer number="42" name="bRestrict" color="1" fill="10" visible="no" active="no"/>
<layer number="43" name="vRestrict" color="2" fill="10" visible="no" active="no"/>
<layer number="44" name="Drills" color="7" fill="1" visible="no" active="no"/>
<layer number="45" name="Holes" color="7" fill="1" visible="no" active="no"/>
<layer number="46" name="Milling" color="3" fill="1" visible="no" active="no"/>
<layer number="47" name="Measures" color="7" fill="1" visible="no" active="no"/>
<layer number="48" name="Document" color="7" fill="1" visible="no" active="no"/>
<layer number="49" name="Reference" color="7" fill="1" visible="no" active="no"/>
<layer number="51" name="tDocu" color="6" fill="1" visible="no" active="no"/>
<layer number="52" name="bDocu" color="7" fill="1" visible="no" active="no"/>
<layer number="88" name="SimResults" color="9" fill="1" visible="yes" active="yes"/>
<layer number="89" name="SimProbes" color="9" fill="1" visible="yes" active="yes"/>
<layer number="90" name="Modules" color="5" fill="1" visible="yes" active="yes"/>
<layer number="91" name="Nets" color="2" fill="1" visible="yes" active="yes"/>
<layer number="92" name="Busses" color="1" fill="1" visible="yes" active="yes"/>
<layer number="93" name="Pins" color="2" fill="1" visible="no" active="yes"/>
<layer number="94" name="Symbols" color="4" fill="1" visible="yes" active="yes"/>
<layer number="95" name="Names" color="7" fill="1" visible="yes" active="yes"/>
<layer number="96" name="Values" color="7" fill="1" visible="yes" active="yes"/>
<layer number="97" name="Info" color="7" fill="1" visible="yes" active="yes"/>
<layer number="98" name="Guide" color="6" fill="1" visible="yes" active="yes"/>
</layers>
<schematic xreflabel="%F%N/%S.%C%R" xrefpart="/%S.%C%R">
<libraries>
<library name="display-kingbright" urn="urn:adsk.eagle:library:213">
<description>&lt;b&gt;KINGBRIGHT Numeric Displays&lt;/b&gt;&lt;p&gt;
&lt;author&gt;librarian@cadsoft.de&lt;/author&gt;</description>
<packages>
<package name="SA56-11" urn="urn:adsk.eagle:footprint:13061/1" library_version="1">
<description>&lt;b&gt;Kingbright 14.2mm (0.56 INCH) SINGLE DIGIT NUMERIC DUISPLAY&lt;/b&gt;&lt;p&gt;
Source: SA56-11EWA(Ver1189471036.pdf</description>
<wire x1="6.273" y1="-9.434" x2="6.273" y2="9.434" width="0.1524" layer="21"/>
<wire x1="-6.273" y1="9.434" x2="6.273" y2="9.434" width="0.1524" layer="21"/>
<wire x1="-6.273" y1="9.434" x2="-6.273" y2="-9.434" width="0.1524" layer="21"/>
<wire x1="6.273" y1="-9.434" x2="-6.273" y2="-9.434" width="0.1524" layer="21"/>
<wire x1="2.4372" y1="-6.5358" x2="2.107" y2="-6.866" width="0.4064" layer="51"/>
<circle x="4.572" y="-6.35" radius="0.381" width="0.762" layer="51"/>
<pad name="1" x="-5.08" y="-7.62" drill="0.8" shape="long" rot="R90"/>
<pad name="2" x="-2.54" y="-7.62" drill="0.8" shape="long" rot="R90"/>
<pad name="3" x="0" y="-7.62" drill="0.8" shape="long" rot="R90"/>
<pad name="4" x="2.54" y="-7.62" drill="0.8" shape="long" rot="R90"/>
<pad name="5" x="5.08" y="-7.62" drill="0.8" shape="long" rot="R90"/>
<pad name="6" x="5.08" y="7.62" drill="0.8" shape="long" rot="R90"/>
<pad name="7" x="2.54" y="7.62" drill="0.8" shape="long" rot="R90"/>
<pad name="8" x="0" y="7.62" drill="0.8" shape="long" rot="R90"/>
<pad name="9" x="-2.54" y="7.62" drill="0.8" shape="long" rot="R90"/>
<pad name="10" x="-5.08" y="7.62" drill="0.8" shape="long" rot="R90"/>
<text x="-6.35" y="10.16" size="1.27" layer="25" ratio="10">&gt;NAME</text>
<text x="-6.35" y="-11.43" size="1.27" layer="27" ratio="10">&gt;VALUE</text>
<polygon width="0.4064" layer="51">
<vertex x="-1.425" y="6.1"/>
<vertex x="-1.25" y="7.1"/>
<vertex x="3.675" y="7.1"/>
<vertex x="3.55" y="6.1"/>
</polygon>
<polygon width="0.4064" layer="51">
<vertex x="3.2565" y="1.0555"/>
<vertex x="4.3128" y="7.0544"/>
<vertex x="4.622" y="7.0626" curve="-104.72173"/>
<vertex x="5.2533" y="6.2038"/>
<vertex x="4.2" y="0.5"/>
<vertex x="3.725" y="0.5"/>
</polygon>
<polygon width="0.4064" layer="51">
<vertex x="-2.95" y="1.1"/>
<vertex x="-1.9" y="7.1"/>
<vertex x="-2.175" y="7.1" curve="77.708822"/>
<vertex x="-3.075" y="6.375"/>
<vertex x="-4.1" y="0.5"/>
<vertex x="-3.525" y="0.5"/>
</polygon>
<polygon width="0.4064" layer="51">
<vertex x="-3.05" y="0.025"/>
<vertex x="-2.425" y="0.65"/>
<vertex x="2.7" y="0.65"/>
<vertex x="3.075" y="0.125"/>
<vertex x="2.625" y="-0.35"/>
<vertex x="-2.675" y="-0.35"/>
</polygon>
<polygon width="0.4064" layer="51">
<vertex x="1.5" y="-5.825"/>
<vertex x="1.325" y="-6.825"/>
<vertex x="-3.6" y="-6.825"/>
<vertex x="-3.475" y="-5.825"/>
</polygon>
<polygon width="0.4064" layer="51">
<vertex x="-3.2565" y="-0.7305"/>
<vertex x="-4.2378" y="-6.7794"/>
<vertex x="-4.547" y="-6.7876" curve="-104.72173"/>
<vertex x="-5.1783" y="-5.9288"/>
<vertex x="-4.2" y="-0.175"/>
<vertex x="-3.725" y="-0.175"/>
</polygon>
<polygon width="0.4064" layer="51">
<vertex x="2.975" y="-0.975"/>
<vertex x="1.975" y="-6.825"/>
<vertex x="2.25" y="-6.825" curve="77.708822"/>
<vertex x="3.15" y="-6.1"/>
<vertex x="4.1" y="-0.425"/>
<vertex x="3.525" y="-0.425"/>
</polygon>
</package>
<package name="SA10-21" urn="urn:adsk.eagle:footprint:13062/1" library_version="1">
<description>&lt;b&gt;25.4mm (1.0 INCH) SINGLE DIGIT NUMERIC DISPLAY&lt;/b&gt;&lt;p&gt;
Source: SA10-21EWA(Ver1189472078.10).pdf&lt;br&gt;
&lt;a href="http://www.farnell.com/datasheets/57132.pdf"&gt; Data sheet &lt;/a&gt;</description>
<wire x1="-11.923" y1="16.909" x2="11.923" y2="16.909" width="0.1524" layer="21"/>
<wire x1="11.923" y1="-16.884" x2="-11.923" y2="-16.884" width="0.1524" layer="21"/>
<wire x1="-11.923" y1="16.909" x2="-11.923" y2="-16.884" width="0.1524" layer="21"/>
<wire x1="11.923" y1="-16.884" x2="11.923" y2="16.909" width="0.1524" layer="21"/>
<circle x="9.05" y="-12.9" radius="1.125" width="0" layer="21"/>
<pad name="1" x="-5.08" y="-15.2" drill="0.8" shape="long" rot="R90"/>
<pad name="2" x="-2.54" y="-15.2" drill="0.8" shape="long" rot="R90"/>
<pad name="3" x="0" y="-15.2" drill="0.8" shape="long" rot="R90"/>
<pad name="4" x="2.54" y="-15.2" drill="0.8" shape="long" rot="R90"/>
<pad name="5" x="5.08" y="-15.2" drill="0.8" shape="long" rot="R90"/>
<pad name="6" x="5.08" y="15.28" drill="0.8" shape="long" rot="R270"/>
<pad name="7" x="2.54" y="15.28" drill="0.8" shape="long" rot="R270"/>
<pad name="8" x="0" y="15.28" drill="0.8" shape="long" rot="R270"/>
<pad name="9" x="-2.54" y="15.28" drill="0.8" shape="long" rot="R270"/>
<pad name="10" x="-5.08" y="15.28" drill="0.8" shape="long" rot="R270"/>
<text x="-11.43" y="17.78" size="1.27" layer="25" ratio="10">&gt;NAME</text>
<text x="-9.525" y="-1.905" size="1.27" layer="27" ratio="10" rot="R90">&gt;VALUE</text>
<polygon width="0.7314" layer="21" spacing="2.286">
<vertex x="-2.565" y="10.98"/>
<vertex x="-2.25" y="12.78"/>
<vertex x="6.615" y="12.78"/>
<vertex x="6.39" y="10.98"/>
</polygon>
<polygon width="0.7314" layer="21" spacing="2.286">
<vertex x="5.8617" y="1.8999"/>
<vertex x="7.763" y="12.6979"/>
<vertex x="8.3196" y="12.7127" curve="-104.72574"/>
<vertex x="9.4559" y="11.1668"/>
<vertex x="7.56" y="0.9"/>
<vertex x="6.705" y="0.9"/>
</polygon>
<polygon width="0.7314" layer="21" spacing="2.286">
<vertex x="-5.31" y="1.98"/>
<vertex x="-3.42" y="12.78"/>
<vertex x="-3.915" y="12.78" curve="77.707709"/>
<vertex x="-5.535" y="11.475"/>
<vertex x="-7.38" y="0.9"/>
<vertex x="-6.345" y="0.9"/>
</polygon>
<polygon width="0.7314" layer="21" spacing="2.286">
<vertex x="-5.49" y="0.045"/>
<vertex x="-4.365" y="1.17"/>
<vertex x="4.86" y="1.17"/>
<vertex x="5.535" y="0.225"/>
<vertex x="4.725" y="-0.63"/>
<vertex x="-4.815" y="-0.63"/>
</polygon>
<polygon width="0.7314" layer="21" spacing="2.286">
<vertex x="2.7" y="-10.485"/>
<vertex x="2.385" y="-12.285"/>
<vertex x="-6.48" y="-12.285"/>
<vertex x="-6.255" y="-10.485"/>
</polygon>
<polygon width="0.7314" layer="21" spacing="2.286">
<vertex x="-5.8617" y="-1.3149"/>
<vertex x="-7.628" y="-12.2029"/>
<vertex x="-8.1846" y="-12.2177" curve="-104.72574"/>
<vertex x="-9.3209" y="-10.6718"/>
<vertex x="-7.56" y="-0.315"/>
<vertex x="-6.705" y="-0.315"/>
</polygon>
<polygon width="0.7314" layer="21" spacing="2.286">
<vertex x="5.355" y="-1.755"/>
<vertex x="3.555" y="-12.285"/>
<vertex x="4.05" y="-12.285" curve="77.707709"/>
<vertex x="5.67" y="-10.98"/>
<vertex x="7.38" y="-0.765"/>
<vertex x="6.345" y="-0.765"/>
</polygon>
</package>
<package name="SA18-11" urn="urn:adsk.eagle:footprint:13063/1" library_version="1">
<description>&lt;b&gt;44.5mm (1.75INCH) SINGLE DIGIT NUMERIC DISPLAY&lt;/b&gt;&lt;p&gt;
Source: SA18-11EWA(Ver1189472866.5).pdf&lt;br&gt;
&lt;a href="http://www.farnell.com/datasheets/58023.pdf"&gt; Data sheet &lt;/a&gt;</description>
<wire x1="-18.875" y1="27.875" x2="18.875" y2="27.875" width="0.2498" layer="21"/>
<wire x1="18.875" y1="-27.875" x2="-18.875" y2="-27.875" width="0.2498" layer="21"/>
<wire x1="-18.875" y1="27.875" x2="-18.875" y2="-27.875" width="0.2498" layer="21"/>
<wire x1="18.875" y1="-27.875" x2="18.875" y2="27.875" width="0.2498" layer="21"/>
<circle x="13.9465" y="-22.9202" radius="2.2035" width="0" layer="21"/>
<pad name="1" x="-5.08" y="-24.13" drill="0.7" diameter="1.4224" shape="long" rot="R90"/>
<pad name="2" x="-2.54" y="-24.13" drill="0.7" diameter="1.4224" shape="long" rot="R90"/>
<pad name="3" x="0" y="-24.13" drill="0.7" diameter="1.4224" shape="long" rot="R90"/>
<pad name="4" x="2.54" y="-24.13" drill="0.7" diameter="1.4224" shape="long" rot="R90"/>
<pad name="5" x="5.08" y="-24.13" drill="0.7" diameter="1.4224" shape="long" rot="R90"/>
<pad name="6" x="5.08" y="24.13" drill="0.7" diameter="1.4224" shape="long" rot="R270"/>
<pad name="7" x="2.54" y="24.13" drill="0.7" diameter="1.4224" shape="long" rot="R270"/>
<pad name="8" x="0" y="24.13" drill="0.7" diameter="1.4224" shape="long" rot="R270"/>
<pad name="9" x="-2.54" y="24.13" drill="0.7" diameter="1.4224" shape="long" rot="R270"/>
<pad name="10" x="-5.08" y="24.13" drill="0.7" diameter="1.4224" shape="long" rot="R270"/>
<text x="-18.7452" y="29.1592" size="2.0828" layer="25" ratio="10">&gt;NAME</text>
<text x="-15.621" y="-3.1242" size="2.0828" layer="27" ratio="10" rot="R90">&gt;VALUE</text>
<polygon width="1.1994" layer="21" spacing="3.749">
<vertex x="-4.2066" y="18.0072"/>
<vertex x="-3.69" y="20.9592"/>
<vertex x="10.8486" y="20.9592"/>
<vertex x="10.4796" y="18.0072"/>
</polygon>
<polygon width="1.1994" layer="21" spacing="3.749">
<vertex x="9.6132" y="3.1158"/>
<vertex x="12.7313" y="20.8246"/>
<vertex x="13.6441" y="20.8488" curve="-104.725326"/>
<vertex x="15.5077" y="18.3136"/>
<vertex x="12.3984" y="1.476"/>
<vertex x="10.9962" y="1.476"/>
</polygon>
<polygon width="1.1994" layer="21" spacing="3.749">
<vertex x="-8.7084" y="3.2472"/>
<vertex x="-5.6088" y="20.9592"/>
<vertex x="-6.4206" y="20.9592" curve="77.708388"/>
<vertex x="-9.0774" y="18.819"/>
<vertex x="-12.1032" y="1.476"/>
<vertex x="-10.4058" y="1.476"/>
</polygon>
<polygon width="1.1994" layer="21" spacing="3.749">
<vertex x="-9.0036" y="0.0738"/>
<vertex x="-7.1586" y="1.9188"/>
<vertex x="7.9704" y="1.9188"/>
<vertex x="9.0774" y="0.369"/>
<vertex x="7.749" y="-1.0332"/>
<vertex x="-7.8966" y="-1.0332"/>
</polygon>
<polygon width="1.1994" layer="21" spacing="3.749">
<vertex x="4.428" y="-17.1954"/>
<vertex x="3.9114" y="-20.1474"/>
<vertex x="-10.6272" y="-20.1474"/>
<vertex x="-10.2582" y="-17.1954"/>
</polygon>
<polygon width="1.1994" layer="21" spacing="3.749">
<vertex x="-9.6132" y="-2.1564"/>
<vertex x="-12.5099" y="-20.0128"/>
<vertex x="-13.4227" y="-20.037" curve="-104.725326"/>
<vertex x="-15.2863" y="-17.5018"/>
<vertex x="-12.3984" y="-0.5166"/>
<vertex x="-10.9962" y="-0.5166"/>
</polygon>
<polygon width="1.1994" layer="21" spacing="3.749">
<vertex x="8.7822" y="-2.8782"/>
<vertex x="5.8302" y="-20.1474"/>
<vertex x="6.642" y="-20.1474" curve="77.708388"/>
<vertex x="9.2988" y="-18.0072"/>
<vertex x="12.1032" y="-1.2546"/>
<vertex x="10.4058" y="-1.2546"/>
</polygon>
</package>
<package name="SA52-11" urn="urn:adsk.eagle:footprint:13064/1" library_version="1">
<description>&lt;b&gt;13.2mm (0.52INCH) SINGLE DIGIT NUMERIC DISPLAY&lt;/b&gt;&lt;p&gt;
Source: http://www.kingbright.com/manager/upload/pdf/SA52-11EWA(Ver1195708215.10)</description>
<wire x1="6.123" y1="-8.684" x2="6.123" y2="8.659" width="0.1524" layer="21"/>
<wire x1="-6.123" y1="8.659" x2="6.123" y2="8.659" width="0.1524" layer="21"/>
<wire x1="-6.123" y1="8.659" x2="-6.123" y2="-8.684" width="0.1524" layer="21"/>
<wire x1="6.123" y1="-8.684" x2="-6.123" y2="-8.684" width="0.1524" layer="21"/>
<circle x="4.072" y="-5.925" radius="0.381" width="0.762" layer="21"/>
<pad name="1" x="-5.08" y="-7.62" drill="0.8" rot="R90"/>
<pad name="2" x="-2.54" y="-7.62" drill="0.8" rot="R90"/>
<pad name="3" x="0" y="-7.62" drill="0.8" rot="R90"/>
<pad name="4" x="2.54" y="-7.62" drill="0.8" rot="R90"/>
<pad name="5" x="5.08" y="-7.62" drill="0.8" rot="R90"/>
<pad name="6" x="5.08" y="7.62" drill="0.8" rot="R90"/>
<pad name="7" x="2.54" y="7.62" drill="0.8" rot="R90"/>
<pad name="8" x="0" y="7.62" drill="0.8" rot="R90"/>
<pad name="9" x="-2.54" y="7.62" drill="0.8" rot="R90"/>
<pad name="10" x="-5.08" y="7.62" drill="0.8" rot="R90"/>
<text x="-6.35" y="10.16" size="1.27" layer="25" ratio="10">&gt;NAME</text>
<text x="-6.35" y="-11.43" size="1.27" layer="27" ratio="10">&gt;VALUE</text>
<polygon width="0.4064" layer="21">
<vertex x="-0.975" y="5.45"/>
<vertex x="-0.8" y="6.45"/>
<vertex x="3" y="6.425"/>
<vertex x="2.875" y="5.425"/>
</polygon>
<polygon width="0.4064" layer="21">
<vertex x="2.7065" y="1.0555"/>
<vertex x="3.6378" y="6.3794"/>
<vertex x="3.947" y="6.3876" curve="-104.72173"/>
<vertex x="4.5783" y="5.5288"/>
<vertex x="3.65" y="0.5"/>
<vertex x="3.175" y="0.5"/>
</polygon>
<polygon width="0.4064" layer="21">
<vertex x="-2.375" y="1.125"/>
<vertex x="-1.45" y="6.45"/>
<vertex x="-1.725" y="6.45" curve="77.708822"/>
<vertex x="-2.625" y="5.725"/>
<vertex x="-3.525" y="0.525"/>
<vertex x="-2.95" y="0.525"/>
</polygon>
<polygon width="0.4064" layer="21">
<vertex x="-2.475" y="0.05"/>
<vertex x="-1.85" y="0.675"/>
<vertex x="2.15" y="0.65"/>
<vertex x="2.525" y="0.125"/>
<vertex x="2.075" y="-0.35"/>
<vertex x="-2.1" y="-0.325"/>
</polygon>
<polygon width="0.4064" layer="21">
<vertex x="1" y="-5.4"/>
<vertex x="0.825" y="-6.4"/>
<vertex x="-2.975" y="-6.375"/>
<vertex x="-2.85" y="-5.375"/>
</polygon>
<polygon width="0.4064" layer="21">
<vertex x="-2.6815" y="-0.7055"/>
<vertex x="-3.6128" y="-6.3294"/>
<vertex x="-3.922" y="-6.3376" curve="-104.72173"/>
<vertex x="-4.5533" y="-5.4788"/>
<vertex x="-3.625" y="-0.15"/>
<vertex x="-3.15" y="-0.15"/>
</polygon>
<polygon width="0.4064" layer="21">
<vertex x="2.425" y="-0.975"/>
<vertex x="1.475" y="-6.4"/>
<vertex x="1.75" y="-6.4" curve="77.708822"/>
<vertex x="2.65" y="-5.675"/>
<vertex x="3.55" y="-0.425"/>
<vertex x="2.975" y="-0.425"/>
</polygon>
</package>
<package name="SA39-11SRWA" urn="urn:adsk.eagle:footprint:13065/1" library_version="1">
<wire x1="4.928" y1="-6.427" x2="-4.928" y2="-6.427" width="0.1524" layer="21"/>
<wire x1="2.921" y1="-4.424" x2="3.175" y2="-4.424" width="0.3048" layer="51"/>
<wire x1="4.928" y1="6.402" x2="4.928" y2="-6.427" width="0.1524" layer="21"/>
<wire x1="-4.928" y1="-6.427" x2="-4.928" y2="6.402" width="0.1524" layer="21"/>
<wire x1="-4.928" y1="6.402" x2="4.928" y2="6.402" width="0.1524" layer="21"/>
<wire x1="3.073" y1="4.1442" x2="2.6412" y2="3.7124" width="0.254" layer="51"/>
<wire x1="2.6412" y1="3.7124" x2="2.0066" y2="0.7366" width="0.254" layer="51"/>
<wire x1="2.0066" y1="0.7366" x2="2.413" y2="0.3302" width="0.254" layer="51"/>
<wire x1="2.413" y1="0.3302" x2="2.794" y2="0.7112" width="0.254" layer="51"/>
<wire x1="2.794" y1="0.7112" x2="3.4032" y2="3.814" width="0.254" layer="51"/>
<wire x1="3.4032" y1="3.814" x2="3.073" y2="4.1442" width="0.254" layer="51"/>
<wire x1="2.7682" y1="4.449" x2="2.3872" y2="4.068" width="0.254" layer="21"/>
<wire x1="2.3872" y1="4.068" x2="-1.0418" y2="4.068" width="0.254" layer="21"/>
<wire x1="-1.0418" y1="4.068" x2="-1.4228" y2="4.449" width="0.254" layer="21"/>
<wire x1="-1.4228" y1="4.449" x2="-1.0418" y2="4.83" width="0.254" layer="21"/>
<wire x1="-1.0418" y1="4.83" x2="2.3872" y2="4.83" width="0.254" layer="21"/>
<wire x1="2.3872" y1="4.83" x2="2.7682" y2="4.449" width="0.254" layer="21"/>
<wire x1="-1.7276" y1="4.1442" x2="-1.2958" y2="3.7124" width="0.254" layer="21"/>
<wire x1="-1.2958" y1="3.7124" x2="-1.905" y2="0.7366" width="0.254" layer="21"/>
<wire x1="-1.905" y1="0.7366" x2="-2.413" y2="0.2286" width="0.254" layer="21"/>
<wire x1="-2.413" y1="0.2286" x2="-2.667" y2="0.4826" width="0.254" layer="21"/>
<wire x1="-2.667" y1="0.4826" x2="-2.0578" y2="3.814" width="0.254" layer="21"/>
<wire x1="-2.0578" y1="3.814" x2="-1.7276" y2="4.1442" width="0.254" layer="21"/>
<wire x1="-2.1082" y1="-0.0762" x2="-1.651" y2="0.381" width="0.254" layer="21"/>
<wire x1="-1.651" y1="0.381" x2="1.7272" y2="0.381" width="0.254" layer="21"/>
<wire x1="1.7272" y1="0.381" x2="2.0574" y2="0.0508" width="0.254" layer="21"/>
<wire x1="2.0574" y1="0.0508" x2="1.6256" y2="-0.381" width="0.254" layer="21"/>
<wire x1="1.6256" y1="-0.381" x2="-1.8034" y2="-0.381" width="0.254" layer="21"/>
<wire x1="-1.8034" y1="-0.381" x2="-2.1082" y2="-0.0762" width="0.254" layer="21"/>
<wire x1="-2.4638" y1="-0.3302" x2="-2.0828" y2="-0.7112" width="0.254" layer="51"/>
<wire x1="-3.1242" y1="-4.1192" x2="-2.667" y2="-3.662" width="0.254" layer="51"/>
<wire x1="-2.667" y1="-3.662" x2="-2.0828" y2="-0.7112" width="0.254" layer="51"/>
<wire x1="-2.4638" y1="-0.3302" x2="-2.8702" y2="-0.7366" width="0.254" layer="51"/>
<wire x1="-2.8702" y1="-0.7366" x2="-3.4544" y2="-3.789" width="0.254" layer="51"/>
<wire x1="-3.4544" y1="-3.789" x2="-3.1242" y2="-4.1192" width="0.254" layer="51"/>
<wire x1="-2.8194" y1="-4.424" x2="-2.4384" y2="-4.043" width="0.254" layer="21"/>
<wire x1="-2.4384" y1="-4.043" x2="0.9906" y2="-4.043" width="0.254" layer="21"/>
<wire x1="0.9906" y1="-4.043" x2="1.3716" y2="-4.424" width="0.254" layer="21"/>
<wire x1="1.3716" y1="-4.424" x2="0.9906" y2="-4.805" width="0.254" layer="21"/>
<wire x1="0.9906" y1="-4.805" x2="-2.4384" y2="-4.805" width="0.254" layer="21"/>
<wire x1="-2.4384" y1="-4.805" x2="-2.8194" y2="-4.424" width="0.254" layer="21"/>
<wire x1="2.3368" y1="-0.2794" x2="1.8288" y2="-0.7874" width="0.254" layer="21"/>
<wire x1="1.8288" y1="-0.7874" x2="1.2446" y2="-3.6874" width="0.254" layer="21"/>
<wire x1="1.2446" y1="-3.6874" x2="1.6764" y2="-4.1192" width="0.254" layer="21"/>
<wire x1="1.6764" y1="-4.1192" x2="2.0066" y2="-3.789" width="0.254" layer="21"/>
<wire x1="2.0066" y1="-3.789" x2="2.5908" y2="-0.5334" width="0.254" layer="21"/>
<wire x1="2.5908" y1="-0.5334" x2="2.3368" y2="-0.2794" width="0.254" layer="21"/>
<wire x1="-2.413" y1="-4.424" x2="1.016" y2="-4.424" width="0.6096" layer="21"/>
<wire x1="1.651" y1="-3.662" x2="2.286" y2="-0.635" width="0.6096" layer="21"/>
<wire x1="1.651" y1="0" x2="-1.778" y2="0" width="0.6096" layer="21"/>
<wire x1="-2.413" y1="-0.635" x2="-3.048" y2="-3.662" width="0.6096" layer="51"/>
<wire x1="-2.311" y1="0.66" x2="-1.678" y2="3.712" width="0.6096" layer="21"/>
<wire x1="-0.991" y1="4.449" x2="2.438" y2="4.449" width="0.6096" layer="21"/>
<wire x1="3.073" y1="3.814" x2="2.413" y2="0.762" width="0.6096" layer="51"/>
<circle x="3.048" y="-4.424" radius="0.254" width="0.6096" layer="51"/>
<pad name="1" x="-3.81" y="5.08" drill="0.9" diameter="1.5"/>
<pad name="2" x="-3.81" y="2.54" drill="0.9" diameter="1.5"/>
<pad name="3" x="-3.81" y="0" drill="0.9" diameter="1.5"/>
<pad name="4" x="-3.81" y="-2.54" drill="0.9" diameter="1.5"/>
<pad name="5" x="-3.81" y="-5.08" drill="0.9" diameter="1.5"/>
<pad name="6" x="3.81" y="-5.08" drill="0.9" diameter="1.5"/>
<pad name="7" x="3.81" y="-2.54" drill="0.9" diameter="1.5"/>
<pad name="8" x="3.81" y="0" drill="0.9" diameter="1.5"/>
<pad name="9" x="3.81" y="2.54" drill="0.9" diameter="1.5"/>
<pad name="10" x="3.81" y="5.08" drill="0.9" diameter="1.5"/>
<text x="-4.953" y="6.9342" size="1.27" layer="25" ratio="10">&gt;NAME</text>
<text x="-4.953" y="-8.1788" size="1.27" layer="27" ratio="10">&gt;VALUE</text>
</package>
</packages>
<packages3d>
<package3d name="SA56-11" urn="urn:adsk.eagle:package:13076/1" type="box" library_version="1">
<description>Kingbright 14.2mm (0.56 INCH) SINGLE DIGIT NUMERIC DUISPLAY
Source: SA56-11EWA(Ver1189471036.pdf</description>
<packageinstances>
<packageinstance name="SA56-11"/>
</packageinstances>
</package3d>
<package3d name="SA10-21" urn="urn:adsk.eagle:package:13078/1" type="box" library_version="1">
<description>25.4mm (1.0 INCH) SINGLE DIGIT NUMERIC DISPLAY
Source: SA10-21EWA(Ver1189472078.10).pdf
 Data sheet </description>
<packageinstances>
<packageinstance name="SA10-21"/>
</packageinstances>
</package3d>
<package3d name="SA18-11" urn="urn:adsk.eagle:package:13077/1" type="box" library_version="1">
<description>44.5mm (1.75INCH) SINGLE DIGIT NUMERIC DISPLAY
Source: SA18-11EWA(Ver1189472866.5).pdf
 Data sheet </description>
<packageinstances>
<packageinstance name="SA18-11"/>
</packageinstances>
</package3d>
<package3d name="SA52-11" urn="urn:adsk.eagle:package:13081/1" type="box" library_version="1">
<description>13.2mm (0.52INCH) SINGLE DIGIT NUMERIC DISPLAY
Source: http://www.kingbright.com/manager/upload/pdf/SA52-11EWA(Ver1195708215.10)</description>
<packageinstances>
<packageinstance name="SA52-11"/>
</packageinstances>
</package3d>
<package3d name="SA39-11SRWA" urn="urn:adsk.eagle:package:13079/1" type="box" library_version="1">
<packageinstances>
<packageinstance name="SA39-11SRWA"/>
</packageinstances>
</package3d>
</packages3d>
<symbols>
<symbol name="7SEG-LED-COM2" urn="urn:adsk.eagle:symbol:13060/1" library_version="1">
<wire x1="2.794" y1="-3.683" x2="3.048" y2="-3.683" width="0.3048" layer="94"/>
<wire x1="2.3368" y1="3.1242" x2="2.032" y2="2.8194" width="0.254" layer="94"/>
<wire x1="2.032" y1="2.8194" x2="1.6256" y2="0.6096" width="0.254" layer="94"/>
<wire x1="1.6256" y1="0.6096" x2="1.905" y2="0.3302" width="0.254" layer="94"/>
<wire x1="1.905" y1="0.3302" x2="2.159" y2="0.5842" width="0.254" layer="94"/>
<wire x1="2.159" y1="0.5842" x2="2.54" y2="2.921" width="0.254" layer="94"/>
<wire x1="2.54" y1="2.921" x2="2.3368" y2="3.1242" width="0.254" layer="94"/>
<wire x1="2.032" y1="3.429" x2="1.778" y2="3.175" width="0.254" layer="94"/>
<wire x1="1.778" y1="3.175" x2="-0.762" y2="3.175" width="0.254" layer="94"/>
<wire x1="-0.762" y1="3.175" x2="-1.016" y2="3.429" width="0.254" layer="94"/>
<wire x1="-1.016" y1="3.429" x2="-0.762" y2="3.683" width="0.254" layer="94"/>
<wire x1="-0.762" y1="3.683" x2="1.778" y2="3.683" width="0.254" layer="94"/>
<wire x1="1.778" y1="3.683" x2="2.032" y2="3.429" width="0.254" layer="94"/>
<wire x1="-1.3208" y1="3.1242" x2="-1.016" y2="2.8194" width="0.254" layer="94"/>
<wire x1="-1.016" y1="2.8194" x2="-1.397" y2="0.6096" width="0.254" layer="94"/>
<wire x1="-1.397" y1="0.6096" x2="-1.651" y2="0.3556" width="0.254" layer="94"/>
<wire x1="-1.651" y1="0.3556" x2="-1.905" y2="0.6096" width="0.254" layer="94"/>
<wire x1="-1.905" y1="0.6096" x2="-1.524" y2="2.921" width="0.254" layer="94"/>
<wire x1="-1.524" y1="2.921" x2="-1.3208" y2="3.1242" width="0.254" layer="94"/>
<wire x1="-1.4732" y1="-0.0762" x2="-1.143" y2="0.254" width="0.254" layer="94"/>
<wire x1="-1.143" y1="0.254" x2="1.3462" y2="0.254" width="0.254" layer="94"/>
<wire x1="1.3462" y1="0.254" x2="1.5494" y2="0.0508" width="0.254" layer="94"/>
<wire x1="1.5494" y1="0.0508" x2="1.2446" y2="-0.254" width="0.254" layer="94"/>
<wire x1="1.2446" y1="-0.254" x2="-1.2954" y2="-0.254" width="0.254" layer="94"/>
<wire x1="-1.2954" y1="-0.254" x2="-1.4732" y2="-0.0762" width="0.254" layer="94"/>
<wire x1="-1.8288" y1="-0.3302" x2="-1.5748" y2="-0.5842" width="0.254" layer="94"/>
<wire x1="-2.286" y1="-3.1242" x2="-1.9558" y2="-2.794" width="0.254" layer="94"/>
<wire x1="-1.9558" y1="-2.794" x2="-1.5748" y2="-0.5842" width="0.254" layer="94"/>
<wire x1="-1.8288" y1="-0.3302" x2="-2.1082" y2="-0.6096" width="0.254" layer="94"/>
<wire x1="-2.1082" y1="-0.6096" x2="-2.4892" y2="-2.921" width="0.254" layer="94"/>
<wire x1="-2.4892" y1="-2.921" x2="-2.286" y2="-3.1242" width="0.254" layer="94"/>
<wire x1="-1.9812" y1="-3.429" x2="-1.7272" y2="-3.175" width="0.254" layer="94"/>
<wire x1="-1.7272" y1="-3.175" x2="0.8128" y2="-3.175" width="0.254" layer="94"/>
<wire x1="0.8128" y1="-3.175" x2="1.0668" y2="-3.429" width="0.254" layer="94"/>
<wire x1="1.0668" y1="-3.429" x2="0.8128" y2="-3.683" width="0.254" layer="94"/>
<wire x1="0.8128" y1="-3.683" x2="-1.7272" y2="-3.683" width="0.254" layer="94"/>
<wire x1="-1.7272" y1="-3.683" x2="-1.9812" y2="-3.429" width="0.254" layer="94"/>
<wire x1="1.7018" y1="-0.4064" x2="1.4478" y2="-0.6604" width="0.254" layer="94"/>
<wire x1="1.4478" y1="-0.6604" x2="1.0668" y2="-2.8194" width="0.254" layer="94"/>
<wire x1="1.0668" y1="-2.8194" x2="1.3716" y2="-3.1242" width="0.254" layer="94"/>
<wire x1="1.3716" y1="-3.1242" x2="1.5748" y2="-2.921" width="0.254" layer="94"/>
<wire x1="1.5748" y1="-2.921" x2="1.9558" y2="-0.6604" width="0.254" layer="94"/>
<wire x1="1.9558" y1="-0.6604" x2="1.7018" y2="-0.4064" width="0.254" layer="94"/>
<wire x1="2.286" y1="2.794" x2="1.905" y2="0.635" width="0.4064" layer="94"/>
<wire x1="1.778" y1="3.429" x2="-0.762" y2="3.429" width="0.4064" layer="94"/>
<wire x1="-1.27" y1="2.794" x2="-1.651" y2="0.635" width="0.4064" layer="94"/>
<wire x1="-1.27" y1="0" x2="1.27" y2="0" width="0.4064" layer="94"/>
<wire x1="1.651" y1="-0.762" x2="1.27" y2="-2.794" width="0.4064" layer="94"/>
<wire x1="0.762" y1="-3.429" x2="-1.651" y2="-3.429" width="0.4064" layer="94"/>
<wire x1="-2.286" y1="-2.921" x2="-1.905" y2="-0.635" width="0.4064" layer="94"/>
<wire x1="5.969" y1="8.89" x2="5.969" y2="7.62" width="0.4064" layer="94"/>
<wire x1="5.969" y1="7.62" x2="5.969" y2="5.08" width="0.4064" layer="94"/>
<wire x1="5.969" y1="5.08" x2="5.969" y2="-7.62" width="0.4064" layer="94"/>
<wire x1="5.969" y1="-7.62" x2="5.969" y2="-8.89" width="0.4064" layer="94"/>
<wire x1="-5.08" y1="-8.89" x2="5.969" y2="-8.89" width="0.4064" layer="94"/>
<wire x1="-5.08" y1="-8.89" x2="-5.08" y2="8.89" width="0.4064" layer="94"/>
<wire x1="5.969" y1="8.89" x2="-5.08" y2="8.89" width="0.4064" layer="94"/>
<wire x1="7.62" y1="7.62" x2="5.969" y2="7.62" width="0.1524" layer="94"/>
<wire x1="7.62" y1="5.08" x2="5.969" y2="5.08" width="0.1524" layer="94"/>
<wire x1="7.62" y1="-7.62" x2="5.969" y2="-7.62" width="0.1524" layer="94"/>
<circle x="2.921" y="-3.683" radius="0.254" width="0.3048" layer="94"/>
<text x="-5.08" y="9.525" size="1.778" layer="95">&gt;NAME</text>
<text x="-5.08" y="-11.43" size="1.778" layer="96">&gt;VALUE</text>
<text x="-6.477" y="7.874" size="1.27" layer="95">a</text>
<text x="-6.477" y="5.334" size="1.27" layer="95">b</text>
<text x="-6.477" y="2.794" size="1.27" layer="95">c</text>
<text x="-6.477" y="0.254" size="1.27" layer="95">d</text>
<text x="-6.477" y="-2.286" size="1.27" layer="95">e</text>
<text x="-6.477" y="-4.826" size="1.27" layer="95">f</text>
<text x="-6.477" y="-7.366" size="1.27" layer="95">g</text>
<text x="0.508" y="6.858" size="1.524" layer="95">COM</text>
<pin name="DP" x="10.16" y="-7.62" length="short" direction="pas" rot="R180"/>
<pin name="F" x="-10.16" y="-5.08" visible="pad" length="middle" direction="pas"/>
<pin name="D" x="-10.16" y="0" visible="pad" length="middle" direction="pas"/>
<pin name="B" x="-10.16" y="5.08" visible="pad" length="middle" direction="pas"/>
<pin name="A" x="-10.16" y="7.62" visible="pad" length="middle" direction="pas"/>
<pin name="COM@1" x="10.16" y="7.62" visible="pad" length="short" direction="pas" rot="R180"/>
<pin name="C" x="-10.16" y="2.54" visible="pad" length="middle" direction="pas"/>
<pin name="E" x="-10.16" y="-2.54" visible="pad" length="middle" direction="pas"/>
<pin name="G" x="-10.16" y="-7.62" visible="pad" length="middle" direction="pas"/>
<pin name="COM@2" x="10.16" y="5.08" visible="pad" length="short" direction="pas" rot="R180"/>
</symbol>
</symbols>
<devicesets>
<deviceset name="7-SEG_" urn="urn:adsk.eagle:component:13094/1" prefix="LED" library_version="1">
<description>&lt;b&gt;Kingbright 14.2mm (0.56 INCH) SINGLE DIGIT NUMERIC DUISPLAY&lt;/b&gt;&lt;p&gt;
Source: SA56-11EWA(Ver1189471036.pdf</description>
<gates>
<gate name="G$1" symbol="7SEG-LED-COM2" x="0" y="0"/>
</gates>
<devices>
<device name="SA56-11" package="SA56-11">
<connects>
<connect gate="G$1" pin="A" pad="7"/>
<connect gate="G$1" pin="B" pad="6"/>
<connect gate="G$1" pin="C" pad="4"/>
<connect gate="G$1" pin="COM@1" pad="3"/>
<connect gate="G$1" pin="COM@2" pad="8"/>
<connect gate="G$1" pin="D" pad="2"/>
<connect gate="G$1" pin="DP" pad="5"/>
<connect gate="G$1" pin="E" pad="1"/>
<connect gate="G$1" pin="F" pad="9"/>
<connect gate="G$1" pin="G" pad="10"/>
</connects>
<package3dinstances>
<package3dinstance package3d_urn="urn:adsk.eagle:package:13076/1"/>
</package3dinstances>
<technologies>
<technology name="">
<attribute name="MF" value="" constant="no"/>
<attribute name="MPN" value="" constant="no"/>
<attribute name="OC_FARNELL" value="unknown" constant="no"/>
<attribute name="OC_NEWARK" value="unknown" constant="no"/>
</technology>
</technologies>
</device>
<device name="SA10-21" package="SA10-21">
<connects>
<connect gate="G$1" pin="A" pad="7"/>
<connect gate="G$1" pin="B" pad="6"/>
<connect gate="G$1" pin="C" pad="4"/>
<connect gate="G$1" pin="COM@1" pad="3"/>
<connect gate="G$1" pin="COM@2" pad="8"/>
<connect gate="G$1" pin="D" pad="2"/>
<connect gate="G$1" pin="DP" pad="5"/>
<connect gate="G$1" pin="E" pad="1"/>
<connect gate="G$1" pin="F" pad="9"/>
<connect gate="G$1" pin="G" pad="10"/>
</connects>
<package3dinstances>
<package3dinstance package3d_urn="urn:adsk.eagle:package:13078/1"/>
</package3dinstances>
<technologies>
<technology name="">
<attribute name="MF" value="" constant="no"/>
<attribute name="MPN" value="" constant="no"/>
<attribute name="OC_FARNELL" value="unknown" constant="no"/>
<attribute name="OC_NEWARK" value="unknown" constant="no"/>
</technology>
</technologies>
</device>
<device name="SA18-11" package="SA18-11">
<connects>
<connect gate="G$1" pin="A" pad="7"/>
<connect gate="G$1" pin="B" pad="6"/>
<connect gate="G$1" pin="C" pad="4"/>
<connect gate="G$1" pin="COM@1" pad="1"/>
<connect gate="G$1" pin="COM@2" pad="5"/>
<connect gate="G$1" pin="D" pad="3"/>
<connect gate="G$1" pin="DP" pad="8"/>
<connect gate="G$1" pin="E" pad="2"/>
<connect gate="G$1" pin="F" pad="9"/>
<connect gate="G$1" pin="G" pad="10"/>
</connects>
<package3dinstances>
<package3dinstance package3d_urn="urn:adsk.eagle:package:13077/1"/>
</package3dinstances>
<technologies>
<technology name="">
<attribute name="MF" value="" constant="no"/>
<attribute name="MPN" value="" constant="no"/>
<attribute name="OC_FARNELL" value="unknown" constant="no"/>
<attribute name="OC_NEWARK" value="unknown" constant="no"/>
</technology>
</technologies>
</device>
<device name="SA52-11" package="SA52-11">
<connects>
<connect gate="G$1" pin="A" pad="7"/>
<connect gate="G$1" pin="B" pad="6"/>
<connect gate="G$1" pin="C" pad="4"/>
<connect gate="G$1" pin="COM@1" pad="3"/>
<connect gate="G$1" pin="COM@2" pad="8"/>
<connect gate="G$1" pin="D" pad="2"/>
<connect gate="G$1" pin="DP" pad="5"/>
<connect gate="G$1" pin="E" pad="1"/>
<connect gate="G$1" pin="F" pad="9"/>
<connect gate="G$1" pin="G" pad="10"/>
</connects>
<package3dinstances>
<package3dinstance package3d_urn="urn:adsk.eagle:package:13081/1"/>
</package3dinstances>
<technologies>
<technology name="">
<attribute name="MF" value="" constant="no"/>
<attribute name="MPN" value="" constant="no"/>
<attribute name="OC_FARNELL" value="unknown" constant="no"/>
<attribute name="OC_NEWARK" value="unknown" constant="no"/>
</technology>
</technologies>
</device>
<device name="SA39-11SRWA" package="SA39-11SRWA">
<connects>
<connect gate="G$1" pin="A" pad="10"/>
<connect gate="G$1" pin="B" pad="9"/>
<connect gate="G$1" pin="C" pad="7"/>
<connect gate="G$1" pin="COM@1" pad="3"/>
<connect gate="G$1" pin="COM@2" pad="8"/>
<connect gate="G$1" pin="D" pad="5"/>
<connect gate="G$1" pin="DP" pad="6"/>
<connect gate="G$1" pin="E" pad="4"/>
<connect gate="G$1" pin="F" pad="2"/>
<connect gate="G$1" pin="G" pad="1"/>
</connects>
<package3dinstances>
<package3dinstance package3d_urn="urn:adsk.eagle:package:13079/1"/>
</package3dinstances>
<technologies>
<technology name="">
<attribute name="MF" value="" constant="no"/>
<attribute name="MPN" value="" constant="no"/>
<attribute name="OC_FARNELL" value="unknown" constant="no"/>
<attribute name="OC_NEWARK" value="unknown" constant="no"/>
</technology>
</technologies>
</device>
</devices>
</deviceset>
</devicesets>
</library>
</libraries>
<attributes>
</attributes>
<variantdefs>
</variantdefs>
<classes>
<class number="0" name="default" width="0" drill="0">
</class>
</classes>
<parts>
<part name="LED13" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED14" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED15" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED16" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED17" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED18" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED19" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED20" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED21" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED22" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED23" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED24" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED37" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED38" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED39" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED40" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED41" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED42" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED43" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED44" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED45" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED46" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED47" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED48" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED25" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED26" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED27" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED28" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED29" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED30" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED31" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED32" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED33" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED34" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED35" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED36" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED61" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED62" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED63" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED64" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED65" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED66" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED67" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED68" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED69" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED70" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED71" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
<part name="LED72" library="display-kingbright" library_urn="urn:adsk.eagle:library:213" deviceset="7-SEG_" device="SA56-11" package3d_urn="urn:adsk.eagle:package:13076/1"/>
</parts>
<sheets>
<sheet>
<plain>
</plain>
<instances>
<instance part="LED13" gate="G$1" x="419.1" y="241.3" smashed="yes">
<attribute name="NAME" x="414.02" y="250.825" size="1.778" layer="95"/>
<attribute name="VALUE" x="414.02" y="229.87" size="1.778" layer="96"/>
</instance>
<instance part="LED14" gate="G$1" x="419.1" y="213.36" smashed="yes">
<attribute name="NAME" x="414.02" y="222.885" size="1.778" layer="95"/>
<attribute name="VALUE" x="414.02" y="201.93" size="1.778" layer="96"/>
</instance>
<instance part="LED15" gate="G$1" x="419.1" y="185.42" smashed="yes">
<attribute name="NAME" x="414.02" y="194.945" size="1.778" layer="95"/>
<attribute name="VALUE" x="414.02" y="173.99" size="1.778" layer="96"/>
</instance>
<instance part="LED16" gate="G$1" x="419.1" y="157.48" smashed="yes">
<attribute name="NAME" x="414.02" y="167.005" size="1.778" layer="95"/>
<attribute name="VALUE" x="414.02" y="146.05" size="1.778" layer="96"/>
</instance>
<instance part="LED17" gate="G$1" x="419.1" y="129.54" smashed="yes">
<attribute name="NAME" x="414.02" y="139.065" size="1.778" layer="95"/>
<attribute name="VALUE" x="414.02" y="118.11" size="1.778" layer="96"/>
</instance>
<instance part="LED18" gate="G$1" x="419.1" y="99.06" smashed="yes">
<attribute name="NAME" x="414.02" y="108.585" size="1.778" layer="95"/>
<attribute name="VALUE" x="414.02" y="87.63" size="1.778" layer="96"/>
</instance>
<instance part="LED19" gate="G$1" x="477.52" y="241.3" smashed="yes">
<attribute name="NAME" x="472.44" y="250.825" size="1.778" layer="95"/>
<attribute name="VALUE" x="472.44" y="229.87" size="1.778" layer="96"/>
</instance>
<instance part="LED20" gate="G$1" x="477.52" y="213.36" smashed="yes">
<attribute name="NAME" x="472.44" y="222.885" size="1.778" layer="95"/>
<attribute name="VALUE" x="472.44" y="201.93" size="1.778" layer="96"/>
</instance>
<instance part="LED21" gate="G$1" x="477.52" y="185.42" smashed="yes">
<attribute name="NAME" x="472.44" y="194.945" size="1.778" layer="95"/>
<attribute name="VALUE" x="472.44" y="173.99" size="1.778" layer="96"/>
</instance>
<instance part="LED22" gate="G$1" x="477.52" y="157.48" smashed="yes">
<attribute name="NAME" x="472.44" y="167.005" size="1.778" layer="95"/>
<attribute name="VALUE" x="472.44" y="146.05" size="1.778" layer="96"/>
</instance>
<instance part="LED23" gate="G$1" x="477.52" y="129.54" smashed="yes">
<attribute name="NAME" x="472.44" y="139.065" size="1.778" layer="95"/>
<attribute name="VALUE" x="472.44" y="118.11" size="1.778" layer="96"/>
</instance>
<instance part="LED24" gate="G$1" x="477.52" y="99.06" smashed="yes">
<attribute name="NAME" x="472.44" y="108.585" size="1.778" layer="95"/>
<attribute name="VALUE" x="472.44" y="87.63" size="1.778" layer="96"/>
</instance>
<instance part="LED37" gate="G$1" x="561.34" y="241.3" smashed="yes">
<attribute name="NAME" x="556.26" y="250.825" size="1.778" layer="95"/>
<attribute name="VALUE" x="556.26" y="229.87" size="1.778" layer="96"/>
</instance>
<instance part="LED38" gate="G$1" x="561.34" y="213.36" smashed="yes">
<attribute name="NAME" x="556.26" y="222.885" size="1.778" layer="95"/>
<attribute name="VALUE" x="556.26" y="201.93" size="1.778" layer="96"/>
</instance>
<instance part="LED39" gate="G$1" x="561.34" y="185.42" smashed="yes">
<attribute name="NAME" x="556.26" y="194.945" size="1.778" layer="95"/>
<attribute name="VALUE" x="556.26" y="173.99" size="1.778" layer="96"/>
</instance>
<instance part="LED40" gate="G$1" x="561.34" y="157.48" smashed="yes">
<attribute name="NAME" x="556.26" y="167.005" size="1.778" layer="95"/>
<attribute name="VALUE" x="556.26" y="146.05" size="1.778" layer="96"/>
</instance>
<instance part="LED41" gate="G$1" x="561.34" y="129.54" smashed="yes">
<attribute name="NAME" x="556.26" y="139.065" size="1.778" layer="95"/>
<attribute name="VALUE" x="556.26" y="118.11" size="1.778" layer="96"/>
</instance>
<instance part="LED42" gate="G$1" x="561.34" y="99.06" smashed="yes">
<attribute name="NAME" x="556.26" y="108.585" size="1.778" layer="95"/>
<attribute name="VALUE" x="556.26" y="87.63" size="1.778" layer="96"/>
</instance>
<instance part="LED43" gate="G$1" x="619.76" y="241.3" smashed="yes">
<attribute name="NAME" x="614.68" y="250.825" size="1.778" layer="95"/>
<attribute name="VALUE" x="614.68" y="229.87" size="1.778" layer="96"/>
</instance>
<instance part="LED44" gate="G$1" x="619.76" y="213.36" smashed="yes">
<attribute name="NAME" x="614.68" y="222.885" size="1.778" layer="95"/>
<attribute name="VALUE" x="614.68" y="201.93" size="1.778" layer="96"/>
</instance>
<instance part="LED45" gate="G$1" x="619.76" y="185.42" smashed="yes">
<attribute name="NAME" x="614.68" y="194.945" size="1.778" layer="95"/>
<attribute name="VALUE" x="614.68" y="173.99" size="1.778" layer="96"/>
</instance>
<instance part="LED46" gate="G$1" x="619.76" y="157.48" smashed="yes">
<attribute name="NAME" x="614.68" y="167.005" size="1.778" layer="95"/>
<attribute name="VALUE" x="614.68" y="146.05" size="1.778" layer="96"/>
</instance>
<instance part="LED47" gate="G$1" x="619.76" y="129.54" smashed="yes">
<attribute name="NAME" x="614.68" y="139.065" size="1.778" layer="95"/>
<attribute name="VALUE" x="614.68" y="118.11" size="1.778" layer="96"/>
</instance>
<instance part="LED48" gate="G$1" x="619.76" y="99.06" smashed="yes">
<attribute name="NAME" x="614.68" y="108.585" size="1.778" layer="95"/>
<attribute name="VALUE" x="614.68" y="87.63" size="1.778" layer="96"/>
</instance>
<instance part="LED25" gate="G$1" x="711.2" y="241.3" smashed="yes">
<attribute name="NAME" x="706.12" y="250.825" size="1.778" layer="95"/>
<attribute name="VALUE" x="706.12" y="229.87" size="1.778" layer="96"/>
</instance>
<instance part="LED26" gate="G$1" x="711.2" y="213.36" smashed="yes">
<attribute name="NAME" x="706.12" y="222.885" size="1.778" layer="95"/>
<attribute name="VALUE" x="706.12" y="201.93" size="1.778" layer="96"/>
</instance>
<instance part="LED27" gate="G$1" x="711.2" y="185.42" smashed="yes">
<attribute name="NAME" x="706.12" y="194.945" size="1.778" layer="95"/>
<attribute name="VALUE" x="706.12" y="173.99" size="1.778" layer="96"/>
</instance>
<instance part="LED28" gate="G$1" x="711.2" y="157.48" smashed="yes">
<attribute name="NAME" x="706.12" y="167.005" size="1.778" layer="95"/>
<attribute name="VALUE" x="706.12" y="146.05" size="1.778" layer="96"/>
</instance>
<instance part="LED29" gate="G$1" x="711.2" y="129.54" smashed="yes">
<attribute name="NAME" x="706.12" y="139.065" size="1.778" layer="95"/>
<attribute name="VALUE" x="706.12" y="118.11" size="1.778" layer="96"/>
</instance>
<instance part="LED30" gate="G$1" x="711.2" y="99.06" smashed="yes">
<attribute name="NAME" x="706.12" y="108.585" size="1.778" layer="95"/>
<attribute name="VALUE" x="706.12" y="87.63" size="1.778" layer="96"/>
</instance>
<instance part="LED31" gate="G$1" x="769.62" y="241.3" smashed="yes">
<attribute name="NAME" x="764.54" y="250.825" size="1.778" layer="95"/>
<attribute name="VALUE" x="764.54" y="229.87" size="1.778" layer="96"/>
</instance>
<instance part="LED32" gate="G$1" x="769.62" y="213.36" smashed="yes">
<attribute name="NAME" x="764.54" y="222.885" size="1.778" layer="95"/>
<attribute name="VALUE" x="764.54" y="201.93" size="1.778" layer="96"/>
</instance>
<instance part="LED33" gate="G$1" x="769.62" y="185.42" smashed="yes">
<attribute name="NAME" x="764.54" y="194.945" size="1.778" layer="95"/>
<attribute name="VALUE" x="764.54" y="173.99" size="1.778" layer="96"/>
</instance>
<instance part="LED34" gate="G$1" x="769.62" y="157.48" smashed="yes">
<attribute name="NAME" x="764.54" y="167.005" size="1.778" layer="95"/>
<attribute name="VALUE" x="764.54" y="146.05" size="1.778" layer="96"/>
</instance>
<instance part="LED35" gate="G$1" x="769.62" y="129.54" smashed="yes">
<attribute name="NAME" x="764.54" y="139.065" size="1.778" layer="95"/>
<attribute name="VALUE" x="764.54" y="118.11" size="1.778" layer="96"/>
</instance>
<instance part="LED36" gate="G$1" x="769.62" y="99.06" smashed="yes">
<attribute name="NAME" x="764.54" y="108.585" size="1.778" layer="95"/>
<attribute name="VALUE" x="764.54" y="87.63" size="1.778" layer="96"/>
</instance>
<instance part="LED61" gate="G$1" x="863.6" y="241.3" smashed="yes">
<attribute name="NAME" x="858.52" y="250.825" size="1.778" layer="95"/>
<attribute name="VALUE" x="858.52" y="229.87" size="1.778" layer="96"/>
</instance>
<instance part="LED62" gate="G$1" x="863.6" y="213.36" smashed="yes">
<attribute name="NAME" x="858.52" y="222.885" size="1.778" layer="95"/>
<attribute name="VALUE" x="858.52" y="201.93" size="1.778" layer="96"/>
</instance>
<instance part="LED63" gate="G$1" x="863.6" y="185.42" smashed="yes">
<attribute name="NAME" x="858.52" y="194.945" size="1.778" layer="95"/>
<attribute name="VALUE" x="858.52" y="173.99" size="1.778" layer="96"/>
</instance>
<instance part="LED64" gate="G$1" x="863.6" y="157.48" smashed="yes">
<attribute name="NAME" x="858.52" y="167.005" size="1.778" layer="95"/>
<attribute name="VALUE" x="858.52" y="146.05" size="1.778" layer="96"/>
</instance>
<instance part="LED65" gate="G$1" x="863.6" y="129.54" smashed="yes">
<attribute name="NAME" x="858.52" y="139.065" size="1.778" layer="95"/>
<attribute name="VALUE" x="858.52" y="118.11" size="1.778" layer="96"/>
</instance>
<instance part="LED66" gate="G$1" x="863.6" y="99.06" smashed="yes">
<attribute name="NAME" x="858.52" y="108.585" size="1.778" layer="95"/>
<attribute name="VALUE" x="858.52" y="87.63" size="1.778" layer="96"/>
</instance>
<instance part="LED67" gate="G$1" x="922.02" y="241.3" smashed="yes">
<attribute name="NAME" x="916.94" y="250.825" size="1.778" layer="95"/>
<attribute name="VALUE" x="916.94" y="229.87" size="1.778" layer="96"/>
</instance>
<instance part="LED68" gate="G$1" x="922.02" y="213.36" smashed="yes">
<attribute name="NAME" x="916.94" y="222.885" size="1.778" layer="95"/>
<attribute name="VALUE" x="916.94" y="201.93" size="1.778" layer="96"/>
</instance>
<instance part="LED69" gate="G$1" x="922.02" y="185.42" smashed="yes">
<attribute name="NAME" x="916.94" y="194.945" size="1.778" layer="95"/>
<attribute name="VALUE" x="916.94" y="173.99" size="1.778" layer="96"/>
</instance>
<instance part="LED70" gate="G$1" x="922.02" y="157.48" smashed="yes">
<attribute name="NAME" x="916.94" y="167.005" size="1.778" layer="95"/>
<attribute name="VALUE" x="916.94" y="146.05" size="1.778" layer="96"/>
</instance>
<instance part="LED71" gate="G$1" x="922.02" y="129.54" smashed="yes">
<attribute name="NAME" x="916.94" y="139.065" size="1.778" layer="95"/>
<attribute name="VALUE" x="916.94" y="118.11" size="1.778" layer="96"/>
</instance>
<instance part="LED72" gate="G$1" x="922.02" y="99.06" smashed="yes">
<attribute name="NAME" x="916.94" y="108.585" size="1.778" layer="95"/>
<attribute name="VALUE" x="916.94" y="87.63" size="1.778" layer="96"/>
</instance>
</instances>
<busses>
</busses>
<nets>
<net name="ROW1" class="0">
<segment>
<pinref part="LED13" gate="G$1" pin="COM@2"/>
<pinref part="LED13" gate="G$1" pin="COM@1"/>
<wire x1="429.26" y1="246.38" x2="429.26" y2="248.92" width="0.1524" layer="91"/>
<wire x1="429.26" y1="248.92" x2="429.26" y2="254" width="0.1524" layer="91"/>
<junction x="429.26" y="248.92"/>
<wire x1="429.26" y1="254" x2="370.84" y2="254" width="0.1524" layer="91"/>
<pinref part="LED19" gate="G$1" pin="COM@1"/>
<wire x1="370.84" y1="254" x2="487.68" y2="254" width="0.1524" layer="91"/>
<wire x1="487.68" y1="254" x2="487.68" y2="248.92" width="0.1524" layer="91"/>
<pinref part="LED19" gate="G$1" pin="COM@2"/>
<wire x1="487.68" y1="248.92" x2="487.68" y2="246.38" width="0.1524" layer="91"/>
<junction x="487.68" y="248.92"/>
<label x="370.84" y="254" size="1.778" layer="95"/>
</segment>
<segment>
<pinref part="LED37" gate="G$1" pin="COM@2"/>
<pinref part="LED37" gate="G$1" pin="COM@1"/>
<wire x1="571.5" y1="246.38" x2="571.5" y2="248.92" width="0.1524" layer="91"/>
<wire x1="571.5" y1="248.92" x2="571.5" y2="254" width="0.1524" layer="91"/>
<junction x="571.5" y="248.92"/>
<wire x1="571.5" y1="254" x2="513.08" y2="254" width="0.1524" layer="91"/>
<pinref part="LED43" gate="G$1" pin="COM@1"/>
<wire x1="513.08" y1="254" x2="629.92" y2="254" width="0.1524" layer="91"/>
<wire x1="629.92" y1="254" x2="629.92" y2="248.92" width="0.1524" layer="91"/>
<pinref part="LED43" gate="G$1" pin="COM@2"/>
<wire x1="629.92" y1="248.92" x2="629.92" y2="246.38" width="0.1524" layer="91"/>
<junction x="629.92" y="248.92"/>
<label x="513.08" y="254" size="1.778" layer="95"/>
</segment>
<segment>
<pinref part="LED25" gate="G$1" pin="COM@2"/>
<pinref part="LED25" gate="G$1" pin="COM@1"/>
<wire x1="721.36" y1="246.38" x2="721.36" y2="248.92" width="0.1524" layer="91"/>
<wire x1="721.36" y1="248.92" x2="721.36" y2="254" width="0.1524" layer="91"/>
<junction x="721.36" y="248.92"/>
<wire x1="721.36" y1="254" x2="662.94" y2="254" width="0.1524" layer="91"/>
<pinref part="LED31" gate="G$1" pin="COM@1"/>
<wire x1="662.94" y1="254" x2="779.78" y2="254" width="0.1524" layer="91"/>
<wire x1="779.78" y1="254" x2="779.78" y2="248.92" width="0.1524" layer="91"/>
<pinref part="LED31" gate="G$1" pin="COM@2"/>
<wire x1="779.78" y1="248.92" x2="779.78" y2="246.38" width="0.1524" layer="91"/>
<junction x="779.78" y="248.92"/>
<label x="662.94" y="254" size="1.778" layer="95"/>
</segment>
<segment>
<pinref part="LED61" gate="G$1" pin="COM@2"/>
<pinref part="LED61" gate="G$1" pin="COM@1"/>
<wire x1="873.76" y1="246.38" x2="873.76" y2="248.92" width="0.1524" layer="91"/>
<wire x1="873.76" y1="248.92" x2="873.76" y2="254" width="0.1524" layer="91"/>
<junction x="873.76" y="248.92"/>
<wire x1="873.76" y1="254" x2="815.34" y2="254" width="0.1524" layer="91"/>
<pinref part="LED67" gate="G$1" pin="COM@1"/>
<wire x1="815.34" y1="254" x2="932.18" y2="254" width="0.1524" layer="91"/>
<wire x1="932.18" y1="254" x2="932.18" y2="248.92" width="0.1524" layer="91"/>
<pinref part="LED67" gate="G$1" pin="COM@2"/>
<wire x1="932.18" y1="248.92" x2="932.18" y2="246.38" width="0.1524" layer="91"/>
<junction x="932.18" y="248.92"/>
<label x="815.34" y="254" size="1.778" layer="95"/>
</segment>
</net>
<net name="ROW2" class="0">
<segment>
<pinref part="LED20" gate="G$1" pin="COM@2"/>
<pinref part="LED20" gate="G$1" pin="COM@1"/>
<wire x1="487.68" y1="218.44" x2="487.68" y2="220.98" width="0.1524" layer="91"/>
<wire x1="487.68" y1="220.98" x2="487.68" y2="226.06" width="0.1524" layer="91"/>
<junction x="487.68" y="220.98"/>
<wire x1="487.68" y1="226.06" x2="429.26" y2="226.06" width="0.1524" layer="91"/>
<pinref part="LED14" gate="G$1" pin="COM@1"/>
<wire x1="429.26" y1="226.06" x2="429.26" y2="220.98" width="0.1524" layer="91"/>
<pinref part="LED14" gate="G$1" pin="COM@2"/>
<wire x1="429.26" y1="220.98" x2="429.26" y2="218.44" width="0.1524" layer="91"/>
<junction x="429.26" y="220.98"/>
<wire x1="429.26" y1="226.06" x2="370.84" y2="226.06" width="0.1524" layer="91"/>
<junction x="429.26" y="226.06"/>
<label x="370.84" y="226.06" size="1.778" layer="95"/>
</segment>
<segment>
<pinref part="LED44" gate="G$1" pin="COM@2"/>
<pinref part="LED44" gate="G$1" pin="COM@1"/>
<wire x1="629.92" y1="218.44" x2="629.92" y2="220.98" width="0.1524" layer="91"/>
<wire x1="629.92" y1="220.98" x2="629.92" y2="226.06" width="0.1524" layer="91"/>
<junction x="629.92" y="220.98"/>
<wire x1="629.92" y1="226.06" x2="571.5" y2="226.06" width="0.1524" layer="91"/>
<pinref part="LED38" gate="G$1" pin="COM@1"/>
<wire x1="571.5" y1="226.06" x2="571.5" y2="220.98" width="0.1524" layer="91"/>
<pinref part="LED38" gate="G$1" pin="COM@2"/>
<wire x1="571.5" y1="220.98" x2="571.5" y2="218.44" width="0.1524" layer="91"/>
<junction x="571.5" y="220.98"/>
<wire x1="571.5" y1="226.06" x2="513.08" y2="226.06" width="0.1524" layer="91"/>
<junction x="571.5" y="226.06"/>
<label x="513.08" y="226.06" size="1.778" layer="95"/>
</segment>
<segment>
<pinref part="LED32" gate="G$1" pin="COM@2"/>
<pinref part="LED32" gate="G$1" pin="COM@1"/>
<wire x1="779.78" y1="218.44" x2="779.78" y2="220.98" width="0.1524" layer="91"/>
<wire x1="779.78" y1="220.98" x2="779.78" y2="226.06" width="0.1524" layer="91"/>
<junction x="779.78" y="220.98"/>
<wire x1="779.78" y1="226.06" x2="721.36" y2="226.06" width="0.1524" layer="91"/>
<pinref part="LED26" gate="G$1" pin="COM@1"/>
<wire x1="721.36" y1="226.06" x2="721.36" y2="220.98" width="0.1524" layer="91"/>
<pinref part="LED26" gate="G$1" pin="COM@2"/>
<wire x1="721.36" y1="220.98" x2="721.36" y2="218.44" width="0.1524" layer="91"/>
<junction x="721.36" y="220.98"/>
<wire x1="721.36" y1="226.06" x2="662.94" y2="226.06" width="0.1524" layer="91"/>
<junction x="721.36" y="226.06"/>
<label x="662.94" y="226.06" size="1.778" layer="95"/>
</segment>
<segment>
<pinref part="LED68" gate="G$1" pin="COM@2"/>
<pinref part="LED68" gate="G$1" pin="COM@1"/>
<wire x1="932.18" y1="218.44" x2="932.18" y2="220.98" width="0.1524" layer="91"/>
<wire x1="932.18" y1="220.98" x2="932.18" y2="226.06" width="0.1524" layer="91"/>
<junction x="932.18" y="220.98"/>
<wire x1="932.18" y1="226.06" x2="873.76" y2="226.06" width="0.1524" layer="91"/>
<pinref part="LED62" gate="G$1" pin="COM@1"/>
<wire x1="873.76" y1="226.06" x2="873.76" y2="220.98" width="0.1524" layer="91"/>
<pinref part="LED62" gate="G$1" pin="COM@2"/>
<wire x1="873.76" y1="220.98" x2="873.76" y2="218.44" width="0.1524" layer="91"/>
<junction x="873.76" y="220.98"/>
<wire x1="873.76" y1="226.06" x2="815.34" y2="226.06" width="0.1524" layer="91"/>
<junction x="873.76" y="226.06"/>
<label x="815.34" y="226.06" size="1.778" layer="95"/>
</segment>
</net>
<net name="ROW3" class="0">
<segment>
<pinref part="LED15" gate="G$1" pin="COM@1"/>
<wire x1="429.26" y1="193.04" x2="429.26" y2="198.12" width="0.1524" layer="91"/>
<wire x1="429.26" y1="198.12" x2="370.84" y2="198.12" width="0.1524" layer="91"/>
<pinref part="LED15" gate="G$1" pin="COM@2"/>
<wire x1="370.84" y1="198.12" x2="429.26" y2="198.12" width="0.1524" layer="91"/>
<wire x1="429.26" y1="198.12" x2="429.26" y2="193.04" width="0.1524" layer="91"/>
<pinref part="LED21" gate="G$1" pin="COM@2"/>
<pinref part="LED21" gate="G$1" pin="COM@1"/>
<wire x1="429.26" y1="193.04" x2="429.26" y2="190.5" width="0.1524" layer="91"/>
<wire x1="487.68" y1="190.5" x2="487.68" y2="193.04" width="0.1524" layer="91"/>
<wire x1="429.26" y1="198.12" x2="487.68" y2="198.12" width="0.1524" layer="91"/>
<wire x1="487.68" y1="198.12" x2="487.68" y2="193.04" width="0.1524" layer="91"/>
<junction x="429.26" y="198.12"/>
<junction x="487.68" y="193.04"/>
<junction x="429.26" y="193.04"/>
<label x="370.84" y="198.12" size="1.778" layer="95"/>
</segment>
<segment>
<pinref part="LED39" gate="G$1" pin="COM@1"/>
<wire x1="571.5" y1="193.04" x2="571.5" y2="198.12" width="0.1524" layer="91"/>
<wire x1="571.5" y1="198.12" x2="513.08" y2="198.12" width="0.1524" layer="91"/>
<pinref part="LED39" gate="G$1" pin="COM@2"/>
<wire x1="513.08" y1="198.12" x2="571.5" y2="198.12" width="0.1524" layer="91"/>
<wire x1="571.5" y1="198.12" x2="571.5" y2="193.04" width="0.1524" layer="91"/>
<pinref part="LED45" gate="G$1" pin="COM@2"/>
<pinref part="LED45" gate="G$1" pin="COM@1"/>
<wire x1="571.5" y1="193.04" x2="571.5" y2="190.5" width="0.1524" layer="91"/>
<wire x1="629.92" y1="190.5" x2="629.92" y2="193.04" width="0.1524" layer="91"/>
<wire x1="571.5" y1="198.12" x2="629.92" y2="198.12" width="0.1524" layer="91"/>
<wire x1="629.92" y1="198.12" x2="629.92" y2="193.04" width="0.1524" layer="91"/>
<junction x="571.5" y="198.12"/>
<junction x="629.92" y="193.04"/>
<junction x="571.5" y="193.04"/>
<label x="513.08" y="198.12" size="1.778" layer="95"/>
</segment>
<segment>
<pinref part="LED27" gate="G$1" pin="COM@1"/>
<wire x1="721.36" y1="193.04" x2="721.36" y2="198.12" width="0.1524" layer="91"/>
<wire x1="721.36" y1="198.12" x2="662.94" y2="198.12" width="0.1524" layer="91"/>
<pinref part="LED27" gate="G$1" pin="COM@2"/>
<wire x1="662.94" y1="198.12" x2="721.36" y2="198.12" width="0.1524" layer="91"/>
<wire x1="721.36" y1="198.12" x2="721.36" y2="193.04" width="0.1524" layer="91"/>
<pinref part="LED33" gate="G$1" pin="COM@2"/>
<pinref part="LED33" gate="G$1" pin="COM@1"/>
<wire x1="721.36" y1="193.04" x2="721.36" y2="190.5" width="0.1524" layer="91"/>
<wire x1="779.78" y1="190.5" x2="779.78" y2="193.04" width="0.1524" layer="91"/>
<wire x1="721.36" y1="198.12" x2="779.78" y2="198.12" width="0.1524" layer="91"/>
<wire x1="779.78" y1="198.12" x2="779.78" y2="193.04" width="0.1524" layer="91"/>
<junction x="721.36" y="198.12"/>
<junction x="779.78" y="193.04"/>
<junction x="721.36" y="193.04"/>
<label x="662.94" y="198.12" size="1.778" layer="95"/>
</segment>
<segment>
<pinref part="LED63" gate="G$1" pin="COM@1"/>
<wire x1="873.76" y1="193.04" x2="873.76" y2="198.12" width="0.1524" layer="91"/>
<wire x1="873.76" y1="198.12" x2="815.34" y2="198.12" width="0.1524" layer="91"/>
<pinref part="LED63" gate="G$1" pin="COM@2"/>
<wire x1="815.34" y1="198.12" x2="873.76" y2="198.12" width="0.1524" layer="91"/>
<wire x1="873.76" y1="198.12" x2="873.76" y2="193.04" width="0.1524" layer="91"/>
<pinref part="LED69" gate="G$1" pin="COM@2"/>
<pinref part="LED69" gate="G$1" pin="COM@1"/>
<wire x1="873.76" y1="193.04" x2="873.76" y2="190.5" width="0.1524" layer="91"/>
<wire x1="932.18" y1="190.5" x2="932.18" y2="193.04" width="0.1524" layer="91"/>
<wire x1="873.76" y1="198.12" x2="932.18" y2="198.12" width="0.1524" layer="91"/>
<wire x1="932.18" y1="198.12" x2="932.18" y2="193.04" width="0.1524" layer="91"/>
<junction x="873.76" y="198.12"/>
<junction x="932.18" y="193.04"/>
<junction x="873.76" y="193.04"/>
<label x="815.34" y="198.12" size="1.778" layer="95"/>
</segment>
</net>
<net name="ROW4" class="0">
<segment>
<pinref part="LED22" gate="G$1" pin="COM@2"/>
<pinref part="LED22" gate="G$1" pin="COM@1"/>
<wire x1="487.68" y1="162.56" x2="487.68" y2="165.1" width="0.1524" layer="91"/>
<wire x1="487.68" y1="165.1" x2="487.68" y2="170.18" width="0.1524" layer="91"/>
<junction x="487.68" y="165.1"/>
<pinref part="LED16" gate="G$1" pin="COM@1"/>
<wire x1="487.68" y1="170.18" x2="429.26" y2="170.18" width="0.1524" layer="91"/>
<wire x1="429.26" y1="170.18" x2="429.26" y2="165.1" width="0.1524" layer="91"/>
<pinref part="LED16" gate="G$1" pin="COM@2"/>
<wire x1="429.26" y1="165.1" x2="429.26" y2="162.56" width="0.1524" layer="91"/>
<junction x="429.26" y="165.1"/>
<wire x1="429.26" y1="170.18" x2="370.84" y2="170.18" width="0.1524" layer="91"/>
<junction x="429.26" y="170.18"/>
<label x="370.84" y="170.18" size="1.778" layer="95"/>
</segment>
<segment>
<pinref part="LED46" gate="G$1" pin="COM@2"/>
<pinref part="LED46" gate="G$1" pin="COM@1"/>
<wire x1="629.92" y1="162.56" x2="629.92" y2="165.1" width="0.1524" layer="91"/>
<wire x1="629.92" y1="165.1" x2="629.92" y2="170.18" width="0.1524" layer="91"/>
<junction x="629.92" y="165.1"/>
<pinref part="LED40" gate="G$1" pin="COM@1"/>
<wire x1="629.92" y1="170.18" x2="571.5" y2="170.18" width="0.1524" layer="91"/>
<wire x1="571.5" y1="170.18" x2="571.5" y2="165.1" width="0.1524" layer="91"/>
<pinref part="LED40" gate="G$1" pin="COM@2"/>
<wire x1="571.5" y1="165.1" x2="571.5" y2="162.56" width="0.1524" layer="91"/>
<junction x="571.5" y="165.1"/>
<wire x1="571.5" y1="170.18" x2="513.08" y2="170.18" width="0.1524" layer="91"/>
<junction x="571.5" y="170.18"/>
<label x="513.08" y="170.18" size="1.778" layer="95"/>
</segment>
<segment>
<pinref part="LED34" gate="G$1" pin="COM@2"/>
<pinref part="LED34" gate="G$1" pin="COM@1"/>
<wire x1="779.78" y1="162.56" x2="779.78" y2="165.1" width="0.1524" layer="91"/>
<wire x1="779.78" y1="165.1" x2="779.78" y2="170.18" width="0.1524" layer="91"/>
<junction x="779.78" y="165.1"/>
<pinref part="LED28" gate="G$1" pin="COM@1"/>
<wire x1="779.78" y1="170.18" x2="721.36" y2="170.18" width="0.1524" layer="91"/>
<wire x1="721.36" y1="170.18" x2="721.36" y2="165.1" width="0.1524" layer="91"/>
<pinref part="LED28" gate="G$1" pin="COM@2"/>
<wire x1="721.36" y1="165.1" x2="721.36" y2="162.56" width="0.1524" layer="91"/>
<junction x="721.36" y="165.1"/>
<wire x1="721.36" y1="170.18" x2="662.94" y2="170.18" width="0.1524" layer="91"/>
<junction x="721.36" y="170.18"/>
<label x="662.94" y="170.18" size="1.778" layer="95"/>
</segment>
<segment>
<pinref part="LED70" gate="G$1" pin="COM@2"/>
<pinref part="LED70" gate="G$1" pin="COM@1"/>
<wire x1="932.18" y1="162.56" x2="932.18" y2="165.1" width="0.1524" layer="91"/>
<wire x1="932.18" y1="165.1" x2="932.18" y2="170.18" width="0.1524" layer="91"/>
<junction x="932.18" y="165.1"/>
<pinref part="LED64" gate="G$1" pin="COM@1"/>
<wire x1="932.18" y1="170.18" x2="873.76" y2="170.18" width="0.1524" layer="91"/>
<wire x1="873.76" y1="170.18" x2="873.76" y2="165.1" width="0.1524" layer="91"/>
<pinref part="LED64" gate="G$1" pin="COM@2"/>
<wire x1="873.76" y1="165.1" x2="873.76" y2="162.56" width="0.1524" layer="91"/>
<junction x="873.76" y="165.1"/>
<wire x1="873.76" y1="170.18" x2="815.34" y2="170.18" width="0.1524" layer="91"/>
<junction x="873.76" y="170.18"/>
<label x="815.34" y="170.18" size="1.778" layer="95"/>
</segment>
</net>
<net name="ROW5" class="0">
<segment>
<wire x1="370.84" y1="142.24" x2="429.26" y2="142.24" width="0.1524" layer="91"/>
<pinref part="LED17" gate="G$1" pin="COM@1"/>
<wire x1="429.26" y1="142.24" x2="429.26" y2="137.16" width="0.1524" layer="91"/>
<pinref part="LED17" gate="G$1" pin="COM@2"/>
<wire x1="429.26" y1="137.16" x2="429.26" y2="134.62" width="0.1524" layer="91"/>
<junction x="429.26" y="137.16"/>
<pinref part="LED23" gate="G$1" pin="COM@1"/>
<wire x1="429.26" y1="142.24" x2="487.68" y2="142.24" width="0.1524" layer="91"/>
<wire x1="487.68" y1="142.24" x2="487.68" y2="137.16" width="0.1524" layer="91"/>
<junction x="429.26" y="142.24"/>
<pinref part="LED23" gate="G$1" pin="COM@2"/>
<wire x1="487.68" y1="137.16" x2="487.68" y2="134.62" width="0.1524" layer="91"/>
<junction x="487.68" y="137.16"/>
<label x="370.84" y="142.24" size="1.778" layer="95"/>
</segment>
<segment>
<wire x1="513.08" y1="142.24" x2="571.5" y2="142.24" width="0.1524" layer="91"/>
<pinref part="LED41" gate="G$1" pin="COM@1"/>
<wire x1="571.5" y1="142.24" x2="571.5" y2="137.16" width="0.1524" layer="91"/>
<pinref part="LED41" gate="G$1" pin="COM@2"/>
<wire x1="571.5" y1="137.16" x2="571.5" y2="134.62" width="0.1524" layer="91"/>
<junction x="571.5" y="137.16"/>
<pinref part="LED47" gate="G$1" pin="COM@1"/>
<wire x1="571.5" y1="142.24" x2="629.92" y2="142.24" width="0.1524" layer="91"/>
<wire x1="629.92" y1="142.24" x2="629.92" y2="137.16" width="0.1524" layer="91"/>
<junction x="571.5" y="142.24"/>
<pinref part="LED47" gate="G$1" pin="COM@2"/>
<wire x1="629.92" y1="137.16" x2="629.92" y2="134.62" width="0.1524" layer="91"/>
<junction x="629.92" y="137.16"/>
<label x="513.08" y="142.24" size="1.778" layer="95"/>
</segment>
<segment>
<wire x1="662.94" y1="142.24" x2="721.36" y2="142.24" width="0.1524" layer="91"/>
<pinref part="LED29" gate="G$1" pin="COM@1"/>
<wire x1="721.36" y1="142.24" x2="721.36" y2="137.16" width="0.1524" layer="91"/>
<pinref part="LED29" gate="G$1" pin="COM@2"/>
<wire x1="721.36" y1="137.16" x2="721.36" y2="134.62" width="0.1524" layer="91"/>
<junction x="721.36" y="137.16"/>
<pinref part="LED35" gate="G$1" pin="COM@1"/>
<wire x1="721.36" y1="142.24" x2="779.78" y2="142.24" width="0.1524" layer="91"/>
<wire x1="779.78" y1="142.24" x2="779.78" y2="137.16" width="0.1524" layer="91"/>
<junction x="721.36" y="142.24"/>
<pinref part="LED35" gate="G$1" pin="COM@2"/>
<wire x1="779.78" y1="137.16" x2="779.78" y2="134.62" width="0.1524" layer="91"/>
<junction x="779.78" y="137.16"/>
<label x="662.94" y="142.24" size="1.778" layer="95"/>
</segment>
<segment>
<wire x1="815.34" y1="142.24" x2="873.76" y2="142.24" width="0.1524" layer="91"/>
<pinref part="LED65" gate="G$1" pin="COM@1"/>
<wire x1="873.76" y1="142.24" x2="873.76" y2="137.16" width="0.1524" layer="91"/>
<pinref part="LED65" gate="G$1" pin="COM@2"/>
<wire x1="873.76" y1="137.16" x2="873.76" y2="134.62" width="0.1524" layer="91"/>
<junction x="873.76" y="137.16"/>
<pinref part="LED71" gate="G$1" pin="COM@1"/>
<wire x1="873.76" y1="142.24" x2="932.18" y2="142.24" width="0.1524" layer="91"/>
<wire x1="932.18" y1="142.24" x2="932.18" y2="137.16" width="0.1524" layer="91"/>
<junction x="873.76" y="142.24"/>
<pinref part="LED71" gate="G$1" pin="COM@2"/>
<wire x1="932.18" y1="137.16" x2="932.18" y2="134.62" width="0.1524" layer="91"/>
<junction x="932.18" y="137.16"/>
<label x="815.34" y="142.24" size="1.778" layer="95"/>
</segment>
</net>
<net name="ROW6" class="0">
<segment>
<pinref part="LED24" gate="G$1" pin="COM@2"/>
<pinref part="LED24" gate="G$1" pin="COM@1"/>
<wire x1="487.68" y1="104.14" x2="487.68" y2="106.68" width="0.1524" layer="91"/>
<wire x1="487.68" y1="106.68" x2="487.68" y2="111.76" width="0.1524" layer="91"/>
<junction x="487.68" y="106.68"/>
<wire x1="487.68" y1="111.76" x2="429.26" y2="111.76" width="0.1524" layer="91"/>
<pinref part="LED18" gate="G$1" pin="COM@1"/>
<wire x1="429.26" y1="106.68" x2="429.26" y2="111.76" width="0.1524" layer="91"/>
<pinref part="LED18" gate="G$1" pin="COM@2"/>
<wire x1="429.26" y1="106.68" x2="429.26" y2="104.14" width="0.1524" layer="91"/>
<junction x="429.26" y="106.68"/>
<wire x1="429.26" y1="111.76" x2="370.84" y2="111.76" width="0.1524" layer="91"/>
<junction x="429.26" y="111.76"/>
<label x="370.84" y="111.76" size="1.778" layer="95"/>
</segment>
<segment>
<pinref part="LED48" gate="G$1" pin="COM@2"/>
<pinref part="LED48" gate="G$1" pin="COM@1"/>
<wire x1="629.92" y1="104.14" x2="629.92" y2="106.68" width="0.1524" layer="91"/>
<wire x1="629.92" y1="106.68" x2="629.92" y2="111.76" width="0.1524" layer="91"/>
<junction x="629.92" y="106.68"/>
<wire x1="629.92" y1="111.76" x2="571.5" y2="111.76" width="0.1524" layer="91"/>
<pinref part="LED42" gate="G$1" pin="COM@1"/>
<wire x1="571.5" y1="106.68" x2="571.5" y2="111.76" width="0.1524" layer="91"/>
<pinref part="LED42" gate="G$1" pin="COM@2"/>
<wire x1="571.5" y1="106.68" x2="571.5" y2="104.14" width="0.1524" layer="91"/>
<junction x="571.5" y="106.68"/>
<wire x1="571.5" y1="111.76" x2="513.08" y2="111.76" width="0.1524" layer="91"/>
<junction x="571.5" y="111.76"/>
<label x="513.08" y="111.76" size="1.778" layer="95"/>
</segment>
<segment>
<pinref part="LED36" gate="G$1" pin="COM@2"/>
<pinref part="LED36" gate="G$1" pin="COM@1"/>
<wire x1="779.78" y1="104.14" x2="779.78" y2="106.68" width="0.1524" layer="91"/>
<wire x1="779.78" y1="106.68" x2="779.78" y2="111.76" width="0.1524" layer="91"/>
<junction x="779.78" y="106.68"/>
<wire x1="779.78" y1="111.76" x2="721.36" y2="111.76" width="0.1524" layer="91"/>
<pinref part="LED30" gate="G$1" pin="COM@1"/>
<wire x1="721.36" y1="106.68" x2="721.36" y2="111.76" width="0.1524" layer="91"/>
<pinref part="LED30" gate="G$1" pin="COM@2"/>
<wire x1="721.36" y1="106.68" x2="721.36" y2="104.14" width="0.1524" layer="91"/>
<junction x="721.36" y="106.68"/>
<wire x1="721.36" y1="111.76" x2="662.94" y2="111.76" width="0.1524" layer="91"/>
<junction x="721.36" y="111.76"/>
<label x="662.94" y="111.76" size="1.778" layer="95"/>
</segment>
<segment>
<pinref part="LED72" gate="G$1" pin="COM@2"/>
<pinref part="LED72" gate="G$1" pin="COM@1"/>
<wire x1="932.18" y1="104.14" x2="932.18" y2="106.68" width="0.1524" layer="91"/>
<wire x1="932.18" y1="106.68" x2="932.18" y2="111.76" width="0.1524" layer="91"/>
<junction x="932.18" y="106.68"/>
<wire x1="932.18" y1="111.76" x2="873.76" y2="111.76" width="0.1524" layer="91"/>
<pinref part="LED66" gate="G$1" pin="COM@1"/>
<wire x1="873.76" y1="106.68" x2="873.76" y2="111.76" width="0.1524" layer="91"/>
<pinref part="LED66" gate="G$1" pin="COM@2"/>
<wire x1="873.76" y1="106.68" x2="873.76" y2="104.14" width="0.1524" layer="91"/>
<junction x="873.76" y="106.68"/>
<wire x1="873.76" y1="111.76" x2="815.34" y2="111.76" width="0.1524" layer="91"/>
<junction x="873.76" y="111.76"/>
<label x="815.34" y="111.76" size="1.778" layer="95"/>
</segment>
</net>
<net name="N$17" class="0">
<segment>
<pinref part="LED13" gate="G$1" pin="A"/>
<wire x1="408.94" y1="248.92" x2="391.16" y2="248.92" width="0.1524" layer="91"/>
<wire x1="391.16" y1="248.92" x2="391.16" y2="220.98" width="0.1524" layer="91"/>
<pinref part="LED14" gate="G$1" pin="A"/>
<wire x1="391.16" y1="220.98" x2="408.94" y2="220.98" width="0.1524" layer="91"/>
<wire x1="391.16" y1="220.98" x2="391.16" y2="193.04" width="0.1524" layer="91"/>
<junction x="391.16" y="220.98"/>
<pinref part="LED15" gate="G$1" pin="A"/>
<wire x1="391.16" y1="193.04" x2="408.94" y2="193.04" width="0.1524" layer="91"/>
<wire x1="391.16" y1="193.04" x2="391.16" y2="165.1" width="0.1524" layer="91"/>
<junction x="391.16" y="193.04"/>
<pinref part="LED16" gate="G$1" pin="A"/>
<wire x1="391.16" y1="165.1" x2="408.94" y2="165.1" width="0.1524" layer="91"/>
<wire x1="391.16" y1="165.1" x2="391.16" y2="137.16" width="0.1524" layer="91"/>
<junction x="391.16" y="165.1"/>
<pinref part="LED17" gate="G$1" pin="A"/>
<wire x1="391.16" y1="137.16" x2="408.94" y2="137.16" width="0.1524" layer="91"/>
<wire x1="391.16" y1="137.16" x2="391.16" y2="106.68" width="0.1524" layer="91"/>
<junction x="391.16" y="137.16"/>
<pinref part="LED18" gate="G$1" pin="A"/>
<wire x1="391.16" y1="106.68" x2="408.94" y2="106.68" width="0.1524" layer="91"/>
</segment>
</net>
<net name="N$18" class="0">
<segment>
<pinref part="LED13" gate="G$1" pin="B"/>
<wire x1="408.94" y1="246.38" x2="393.7" y2="246.38" width="0.1524" layer="91"/>
<wire x1="393.7" y1="246.38" x2="393.7" y2="218.44" width="0.1524" layer="91"/>
<pinref part="LED14" gate="G$1" pin="B"/>
<wire x1="393.7" y1="218.44" x2="408.94" y2="218.44" width="0.1524" layer="91"/>
<wire x1="393.7" y1="218.44" x2="393.7" y2="190.5" width="0.1524" layer="91"/>
<junction x="393.7" y="218.44"/>
<pinref part="LED15" gate="G$1" pin="B"/>
<wire x1="393.7" y1="190.5" x2="408.94" y2="190.5" width="0.1524" layer="91"/>
<wire x1="393.7" y1="190.5" x2="393.7" y2="162.56" width="0.1524" layer="91"/>
<junction x="393.7" y="190.5"/>
<pinref part="LED16" gate="G$1" pin="B"/>
<wire x1="393.7" y1="162.56" x2="408.94" y2="162.56" width="0.1524" layer="91"/>
<wire x1="393.7" y1="162.56" x2="393.7" y2="134.62" width="0.1524" layer="91"/>
<junction x="393.7" y="162.56"/>
<pinref part="LED18" gate="G$1" pin="B"/>
<wire x1="393.7" y1="134.62" x2="393.7" y2="132.08" width="0.1524" layer="91"/>
<wire x1="393.7" y1="132.08" x2="393.7" y2="104.14" width="0.1524" layer="91"/>
<wire x1="393.7" y1="104.14" x2="408.94" y2="104.14" width="0.1524" layer="91"/>
<pinref part="LED17" gate="G$1" pin="B"/>
<wire x1="408.94" y1="134.62" x2="393.7" y2="134.62" width="0.1524" layer="91"/>
<junction x="393.7" y="134.62"/>
</segment>
</net>
<net name="N$19" class="0">
<segment>
<pinref part="LED18" gate="G$1" pin="G"/>
<wire x1="408.94" y1="91.44" x2="406.4" y2="91.44" width="0.1524" layer="91"/>
<wire x1="406.4" y1="91.44" x2="406.4" y2="121.92" width="0.1524" layer="91"/>
<pinref part="LED17" gate="G$1" pin="G"/>
<wire x1="406.4" y1="121.92" x2="408.94" y2="121.92" width="0.1524" layer="91"/>
<wire x1="406.4" y1="121.92" x2="406.4" y2="149.86" width="0.1524" layer="91"/>
<junction x="406.4" y="121.92"/>
<pinref part="LED16" gate="G$1" pin="G"/>
<wire x1="406.4" y1="149.86" x2="408.94" y2="149.86" width="0.1524" layer="91"/>
<wire x1="406.4" y1="149.86" x2="406.4" y2="177.8" width="0.1524" layer="91"/>
<junction x="406.4" y="149.86"/>
<pinref part="LED15" gate="G$1" pin="G"/>
<wire x1="406.4" y1="177.8" x2="408.94" y2="177.8" width="0.1524" layer="91"/>
<pinref part="LED14" gate="G$1" pin="G"/>
<wire x1="406.4" y1="177.8" x2="406.4" y2="205.74" width="0.1524" layer="91"/>
<wire x1="406.4" y1="205.74" x2="408.94" y2="205.74" width="0.1524" layer="91"/>
<junction x="406.4" y="177.8"/>
<pinref part="LED13" gate="G$1" pin="G"/>
<wire x1="406.4" y1="205.74" x2="406.4" y2="233.68" width="0.1524" layer="91"/>
<wire x1="406.4" y1="233.68" x2="408.94" y2="233.68" width="0.1524" layer="91"/>
<junction x="406.4" y="205.74"/>
</segment>
</net>
<net name="N$20" class="0">
<segment>
<pinref part="LED18" gate="G$1" pin="F"/>
<wire x1="408.94" y1="93.98" x2="403.86" y2="93.98" width="0.1524" layer="91"/>
<wire x1="403.86" y1="93.98" x2="403.86" y2="124.46" width="0.1524" layer="91"/>
<pinref part="LED17" gate="G$1" pin="F"/>
<wire x1="403.86" y1="124.46" x2="408.94" y2="124.46" width="0.1524" layer="91"/>
<wire x1="403.86" y1="124.46" x2="403.86" y2="152.4" width="0.1524" layer="91"/>
<junction x="403.86" y="124.46"/>
<pinref part="LED16" gate="G$1" pin="F"/>
<wire x1="403.86" y1="152.4" x2="408.94" y2="152.4" width="0.1524" layer="91"/>
<wire x1="403.86" y1="152.4" x2="403.86" y2="180.34" width="0.1524" layer="91"/>
<junction x="403.86" y="152.4"/>
<pinref part="LED15" gate="G$1" pin="F"/>
<wire x1="403.86" y1="180.34" x2="408.94" y2="180.34" width="0.1524" layer="91"/>
<wire x1="403.86" y1="180.34" x2="403.86" y2="208.28" width="0.1524" layer="91"/>
<junction x="403.86" y="180.34"/>
<pinref part="LED14" gate="G$1" pin="F"/>
<wire x1="403.86" y1="208.28" x2="408.94" y2="208.28" width="0.1524" layer="91"/>
<wire x1="403.86" y1="208.28" x2="403.86" y2="236.22" width="0.1524" layer="91"/>
<junction x="403.86" y="208.28"/>
<pinref part="LED13" gate="G$1" pin="F"/>
<wire x1="403.86" y1="236.22" x2="408.94" y2="236.22" width="0.1524" layer="91"/>
</segment>
</net>
<net name="N$21" class="0">
<segment>
<pinref part="LED18" gate="G$1" pin="E"/>
<wire x1="408.94" y1="96.52" x2="401.32" y2="96.52" width="0.1524" layer="91"/>
<wire x1="401.32" y1="96.52" x2="401.32" y2="127" width="0.1524" layer="91"/>
<pinref part="LED17" gate="G$1" pin="E"/>
<wire x1="401.32" y1="127" x2="408.94" y2="127" width="0.1524" layer="91"/>
<wire x1="401.32" y1="127" x2="401.32" y2="154.94" width="0.1524" layer="91"/>
<junction x="401.32" y="127"/>
<pinref part="LED16" gate="G$1" pin="E"/>
<wire x1="401.32" y1="154.94" x2="408.94" y2="154.94" width="0.1524" layer="91"/>
<wire x1="401.32" y1="154.94" x2="401.32" y2="182.88" width="0.1524" layer="91"/>
<junction x="401.32" y="154.94"/>
<pinref part="LED15" gate="G$1" pin="E"/>
<wire x1="401.32" y1="182.88" x2="408.94" y2="182.88" width="0.1524" layer="91"/>
<pinref part="LED14" gate="G$1" pin="E"/>
<wire x1="408.94" y1="210.82" x2="401.32" y2="210.82" width="0.1524" layer="91"/>
<wire x1="401.32" y1="210.82" x2="401.32" y2="182.88" width="0.1524" layer="91"/>
<junction x="401.32" y="182.88"/>
<pinref part="LED13" gate="G$1" pin="E"/>
<wire x1="408.94" y1="238.76" x2="401.32" y2="238.76" width="0.1524" layer="91"/>
<wire x1="401.32" y1="238.76" x2="401.32" y2="210.82" width="0.1524" layer="91"/>
<junction x="401.32" y="210.82"/>
</segment>
</net>
<net name="N$22" class="0">
<segment>
<pinref part="LED13" gate="G$1" pin="D"/>
<wire x1="408.94" y1="241.3" x2="398.78" y2="241.3" width="0.1524" layer="91"/>
<wire x1="398.78" y1="241.3" x2="398.78" y2="213.36" width="0.1524" layer="91"/>
<pinref part="LED14" gate="G$1" pin="D"/>
<wire x1="398.78" y1="213.36" x2="408.94" y2="213.36" width="0.1524" layer="91"/>
<pinref part="LED15" gate="G$1" pin="D"/>
<wire x1="398.78" y1="213.36" x2="398.78" y2="185.42" width="0.1524" layer="91"/>
<wire x1="398.78" y1="185.42" x2="408.94" y2="185.42" width="0.1524" layer="91"/>
<junction x="398.78" y="213.36"/>
<pinref part="LED16" gate="G$1" pin="D"/>
<wire x1="398.78" y1="185.42" x2="398.78" y2="157.48" width="0.1524" layer="91"/>
<wire x1="398.78" y1="157.48" x2="408.94" y2="157.48" width="0.1524" layer="91"/>
<junction x="398.78" y="185.42"/>
<pinref part="LED17" gate="G$1" pin="D"/>
<wire x1="398.78" y1="157.48" x2="398.78" y2="129.54" width="0.1524" layer="91"/>
<wire x1="398.78" y1="129.54" x2="408.94" y2="129.54" width="0.1524" layer="91"/>
<junction x="398.78" y="157.48"/>
<pinref part="LED18" gate="G$1" pin="D"/>
<wire x1="398.78" y1="129.54" x2="398.78" y2="99.06" width="0.1524" layer="91"/>
<wire x1="398.78" y1="99.06" x2="408.94" y2="99.06" width="0.1524" layer="91"/>
<junction x="398.78" y="129.54"/>
</segment>
</net>
<net name="N$23" class="0">
<segment>
<pinref part="LED13" gate="G$1" pin="C"/>
<wire x1="408.94" y1="243.84" x2="396.24" y2="243.84" width="0.1524" layer="91"/>
<pinref part="LED14" gate="G$1" pin="C"/>
<wire x1="396.24" y1="243.84" x2="396.24" y2="215.9" width="0.1524" layer="91"/>
<wire x1="396.24" y1="215.9" x2="408.94" y2="215.9" width="0.1524" layer="91"/>
<pinref part="LED15" gate="G$1" pin="C"/>
<wire x1="396.24" y1="215.9" x2="396.24" y2="187.96" width="0.1524" layer="91"/>
<wire x1="396.24" y1="187.96" x2="408.94" y2="187.96" width="0.1524" layer="91"/>
<junction x="396.24" y="215.9"/>
<pinref part="LED16" gate="G$1" pin="C"/>
<wire x1="396.24" y1="187.96" x2="396.24" y2="160.02" width="0.1524" layer="91"/>
<wire x1="396.24" y1="160.02" x2="408.94" y2="160.02" width="0.1524" layer="91"/>
<junction x="396.24" y="187.96"/>
<pinref part="LED17" gate="G$1" pin="C"/>
<wire x1="396.24" y1="160.02" x2="396.24" y2="132.08" width="0.1524" layer="91"/>
<wire x1="396.24" y1="132.08" x2="408.94" y2="132.08" width="0.1524" layer="91"/>
<junction x="396.24" y="160.02"/>
<pinref part="LED18" gate="G$1" pin="C"/>
<wire x1="396.24" y1="132.08" x2="396.24" y2="101.6" width="0.1524" layer="91"/>
<wire x1="396.24" y1="101.6" x2="408.94" y2="101.6" width="0.1524" layer="91"/>
<junction x="396.24" y="132.08"/>
</segment>
</net>
<net name="N$24" class="0">
<segment>
<pinref part="LED13" gate="G$1" pin="DP"/>
<wire x1="429.26" y1="233.68" x2="434.34" y2="233.68" width="0.1524" layer="91"/>
<wire x1="434.34" y1="233.68" x2="434.34" y2="205.74" width="0.1524" layer="91"/>
<pinref part="LED14" gate="G$1" pin="DP"/>
<wire x1="434.34" y1="205.74" x2="429.26" y2="205.74" width="0.1524" layer="91"/>
<pinref part="LED15" gate="G$1" pin="DP"/>
<wire x1="434.34" y1="205.74" x2="434.34" y2="177.8" width="0.1524" layer="91"/>
<wire x1="434.34" y1="177.8" x2="429.26" y2="177.8" width="0.1524" layer="91"/>
<junction x="434.34" y="205.74"/>
<pinref part="LED16" gate="G$1" pin="DP"/>
<wire x1="434.34" y1="177.8" x2="434.34" y2="149.86" width="0.1524" layer="91"/>
<wire x1="434.34" y1="149.86" x2="429.26" y2="149.86" width="0.1524" layer="91"/>
<junction x="434.34" y="177.8"/>
<pinref part="LED17" gate="G$1" pin="DP"/>
<wire x1="434.34" y1="149.86" x2="434.34" y2="121.92" width="0.1524" layer="91"/>
<wire x1="434.34" y1="121.92" x2="429.26" y2="121.92" width="0.1524" layer="91"/>
<junction x="434.34" y="149.86"/>
<pinref part="LED18" gate="G$1" pin="DP"/>
<wire x1="434.34" y1="121.92" x2="434.34" y2="91.44" width="0.1524" layer="91"/>
<wire x1="434.34" y1="91.44" x2="429.26" y2="91.44" width="0.1524" layer="91"/>
<junction x="434.34" y="121.92"/>
</segment>
</net>
<net name="N$25" class="0">
<segment>
<pinref part="LED24" gate="G$1" pin="G"/>
<wire x1="467.36" y1="91.44" x2="464.82" y2="91.44" width="0.1524" layer="91"/>
<wire x1="464.82" y1="91.44" x2="464.82" y2="121.92" width="0.1524" layer="91"/>
<pinref part="LED23" gate="G$1" pin="G"/>
<wire x1="467.36" y1="121.92" x2="464.82" y2="121.92" width="0.1524" layer="91"/>
<pinref part="LED22" gate="G$1" pin="G"/>
<wire x1="464.82" y1="121.92" x2="464.82" y2="149.86" width="0.1524" layer="91"/>
<wire x1="464.82" y1="149.86" x2="467.36" y2="149.86" width="0.1524" layer="91"/>
<junction x="464.82" y="121.92"/>
<wire x1="464.82" y1="149.86" x2="464.82" y2="177.8" width="0.1524" layer="91"/>
<junction x="464.82" y="149.86"/>
<pinref part="LED21" gate="G$1" pin="G"/>
<wire x1="464.82" y1="177.8" x2="467.36" y2="177.8" width="0.1524" layer="91"/>
<pinref part="LED20" gate="G$1" pin="G"/>
<wire x1="464.82" y1="177.8" x2="464.82" y2="205.74" width="0.1524" layer="91"/>
<wire x1="464.82" y1="205.74" x2="467.36" y2="205.74" width="0.1524" layer="91"/>
<junction x="464.82" y="177.8"/>
<pinref part="LED19" gate="G$1" pin="G"/>
<wire x1="464.82" y1="205.74" x2="464.82" y2="233.68" width="0.1524" layer="91"/>
<wire x1="464.82" y1="233.68" x2="467.36" y2="233.68" width="0.1524" layer="91"/>
<junction x="464.82" y="205.74"/>
</segment>
</net>
<net name="N$26" class="0">
<segment>
<pinref part="LED24" gate="G$1" pin="F"/>
<wire x1="467.36" y1="93.98" x2="462.28" y2="93.98" width="0.1524" layer="91"/>
<pinref part="LED23" gate="G$1" pin="F"/>
<wire x1="462.28" y1="93.98" x2="462.28" y2="124.46" width="0.1524" layer="91"/>
<wire x1="462.28" y1="124.46" x2="467.36" y2="124.46" width="0.1524" layer="91"/>
<pinref part="LED22" gate="G$1" pin="F"/>
<wire x1="462.28" y1="124.46" x2="462.28" y2="152.4" width="0.1524" layer="91"/>
<wire x1="462.28" y1="152.4" x2="467.36" y2="152.4" width="0.1524" layer="91"/>
<junction x="462.28" y="124.46"/>
<pinref part="LED21" gate="G$1" pin="F"/>
<wire x1="462.28" y1="152.4" x2="462.28" y2="180.34" width="0.1524" layer="91"/>
<wire x1="462.28" y1="180.34" x2="467.36" y2="180.34" width="0.1524" layer="91"/>
<junction x="462.28" y="152.4"/>
<pinref part="LED20" gate="G$1" pin="F"/>
<wire x1="462.28" y1="180.34" x2="462.28" y2="208.28" width="0.1524" layer="91"/>
<wire x1="462.28" y1="208.28" x2="467.36" y2="208.28" width="0.1524" layer="91"/>
<junction x="462.28" y="180.34"/>
<pinref part="LED19" gate="G$1" pin="F"/>
<wire x1="462.28" y1="208.28" x2="462.28" y2="236.22" width="0.1524" layer="91"/>
<wire x1="462.28" y1="236.22" x2="467.36" y2="236.22" width="0.1524" layer="91"/>
<junction x="462.28" y="208.28"/>
</segment>
</net>
<net name="N$27" class="0">
<segment>
<pinref part="LED24" gate="G$1" pin="E"/>
<wire x1="467.36" y1="96.52" x2="459.74" y2="96.52" width="0.1524" layer="91"/>
<pinref part="LED23" gate="G$1" pin="E"/>
<wire x1="459.74" y1="96.52" x2="459.74" y2="127" width="0.1524" layer="91"/>
<wire x1="459.74" y1="127" x2="467.36" y2="127" width="0.1524" layer="91"/>
<pinref part="LED22" gate="G$1" pin="E"/>
<wire x1="459.74" y1="127" x2="459.74" y2="154.94" width="0.1524" layer="91"/>
<wire x1="459.74" y1="154.94" x2="467.36" y2="154.94" width="0.1524" layer="91"/>
<junction x="459.74" y="127"/>
<pinref part="LED21" gate="G$1" pin="E"/>
<wire x1="459.74" y1="154.94" x2="459.74" y2="182.88" width="0.1524" layer="91"/>
<wire x1="459.74" y1="182.88" x2="467.36" y2="182.88" width="0.1524" layer="91"/>
<junction x="459.74" y="154.94"/>
<pinref part="LED20" gate="G$1" pin="E"/>
<wire x1="459.74" y1="182.88" x2="459.74" y2="210.82" width="0.1524" layer="91"/>
<wire x1="459.74" y1="210.82" x2="467.36" y2="210.82" width="0.1524" layer="91"/>
<junction x="459.74" y="182.88"/>
<pinref part="LED19" gate="G$1" pin="E"/>
<wire x1="459.74" y1="210.82" x2="459.74" y2="238.76" width="0.1524" layer="91"/>
<wire x1="459.74" y1="238.76" x2="467.36" y2="238.76" width="0.1524" layer="91"/>
<junction x="459.74" y="210.82"/>
</segment>
</net>
<net name="N$28" class="0">
<segment>
<pinref part="LED24" gate="G$1" pin="D"/>
<wire x1="467.36" y1="99.06" x2="457.2" y2="99.06" width="0.1524" layer="91"/>
<pinref part="LED23" gate="G$1" pin="D"/>
<wire x1="457.2" y1="99.06" x2="457.2" y2="129.54" width="0.1524" layer="91"/>
<wire x1="457.2" y1="129.54" x2="467.36" y2="129.54" width="0.1524" layer="91"/>
<pinref part="LED22" gate="G$1" pin="D"/>
<wire x1="457.2" y1="129.54" x2="457.2" y2="157.48" width="0.1524" layer="91"/>
<wire x1="457.2" y1="157.48" x2="467.36" y2="157.48" width="0.1524" layer="91"/>
<junction x="457.2" y="129.54"/>
<pinref part="LED21" gate="G$1" pin="D"/>
<wire x1="457.2" y1="157.48" x2="457.2" y2="185.42" width="0.1524" layer="91"/>
<wire x1="457.2" y1="185.42" x2="467.36" y2="185.42" width="0.1524" layer="91"/>
<junction x="457.2" y="157.48"/>
<pinref part="LED20" gate="G$1" pin="D"/>
<wire x1="457.2" y1="185.42" x2="457.2" y2="213.36" width="0.1524" layer="91"/>
<wire x1="457.2" y1="213.36" x2="467.36" y2="213.36" width="0.1524" layer="91"/>
<junction x="457.2" y="185.42"/>
<pinref part="LED19" gate="G$1" pin="D"/>
<wire x1="457.2" y1="213.36" x2="457.2" y2="241.3" width="0.1524" layer="91"/>
<wire x1="457.2" y1="241.3" x2="467.36" y2="241.3" width="0.1524" layer="91"/>
<junction x="457.2" y="213.36"/>
</segment>
</net>
<net name="N$29" class="0">
<segment>
<pinref part="LED24" gate="G$1" pin="C"/>
<wire x1="467.36" y1="101.6" x2="454.66" y2="101.6" width="0.1524" layer="91"/>
<pinref part="LED23" gate="G$1" pin="C"/>
<wire x1="454.66" y1="101.6" x2="454.66" y2="132.08" width="0.1524" layer="91"/>
<wire x1="454.66" y1="132.08" x2="467.36" y2="132.08" width="0.1524" layer="91"/>
<pinref part="LED22" gate="G$1" pin="C"/>
<wire x1="454.66" y1="132.08" x2="454.66" y2="160.02" width="0.1524" layer="91"/>
<wire x1="454.66" y1="160.02" x2="467.36" y2="160.02" width="0.1524" layer="91"/>
<junction x="454.66" y="132.08"/>
<pinref part="LED21" gate="G$1" pin="C"/>
<wire x1="454.66" y1="160.02" x2="454.66" y2="187.96" width="0.1524" layer="91"/>
<wire x1="454.66" y1="187.96" x2="467.36" y2="187.96" width="0.1524" layer="91"/>
<junction x="454.66" y="160.02"/>
<pinref part="LED20" gate="G$1" pin="C"/>
<wire x1="454.66" y1="187.96" x2="454.66" y2="215.9" width="0.1524" layer="91"/>
<wire x1="454.66" y1="215.9" x2="467.36" y2="215.9" width="0.1524" layer="91"/>
<junction x="454.66" y="187.96"/>
<pinref part="LED19" gate="G$1" pin="C"/>
<wire x1="454.66" y1="215.9" x2="454.66" y2="243.84" width="0.1524" layer="91"/>
<wire x1="454.66" y1="243.84" x2="467.36" y2="243.84" width="0.1524" layer="91"/>
<junction x="454.66" y="215.9"/>
</segment>
</net>
<net name="N$30" class="0">
<segment>
<pinref part="LED24" gate="G$1" pin="B"/>
<wire x1="467.36" y1="104.14" x2="452.12" y2="104.14" width="0.1524" layer="91"/>
<pinref part="LED23" gate="G$1" pin="B"/>
<wire x1="452.12" y1="104.14" x2="452.12" y2="134.62" width="0.1524" layer="91"/>
<wire x1="452.12" y1="134.62" x2="467.36" y2="134.62" width="0.1524" layer="91"/>
<pinref part="LED22" gate="G$1" pin="B"/>
<wire x1="452.12" y1="134.62" x2="452.12" y2="162.56" width="0.1524" layer="91"/>
<wire x1="452.12" y1="162.56" x2="467.36" y2="162.56" width="0.1524" layer="91"/>
<junction x="452.12" y="134.62"/>
<pinref part="LED21" gate="G$1" pin="B"/>
<wire x1="452.12" y1="162.56" x2="452.12" y2="190.5" width="0.1524" layer="91"/>
<wire x1="452.12" y1="190.5" x2="467.36" y2="190.5" width="0.1524" layer="91"/>
<junction x="452.12" y="162.56"/>
<pinref part="LED20" gate="G$1" pin="B"/>
<wire x1="452.12" y1="190.5" x2="452.12" y2="218.44" width="0.1524" layer="91"/>
<wire x1="452.12" y1="218.44" x2="467.36" y2="218.44" width="0.1524" layer="91"/>
<junction x="452.12" y="190.5"/>
<pinref part="LED19" gate="G$1" pin="B"/>
<wire x1="452.12" y1="218.44" x2="452.12" y2="246.38" width="0.1524" layer="91"/>
<wire x1="452.12" y1="246.38" x2="467.36" y2="246.38" width="0.1524" layer="91"/>
<junction x="452.12" y="218.44"/>
</segment>
</net>
<net name="N$31" class="0">
<segment>
<pinref part="LED24" gate="G$1" pin="A"/>
<wire x1="467.36" y1="106.68" x2="449.58" y2="106.68" width="0.1524" layer="91"/>
<pinref part="LED23" gate="G$1" pin="A"/>
<wire x1="449.58" y1="106.68" x2="449.58" y2="137.16" width="0.1524" layer="91"/>
<wire x1="449.58" y1="137.16" x2="467.36" y2="137.16" width="0.1524" layer="91"/>
<pinref part="LED22" gate="G$1" pin="A"/>
<wire x1="449.58" y1="137.16" x2="449.58" y2="165.1" width="0.1524" layer="91"/>
<wire x1="449.58" y1="165.1" x2="467.36" y2="165.1" width="0.1524" layer="91"/>
<junction x="449.58" y="137.16"/>
<pinref part="LED21" gate="G$1" pin="A"/>
<wire x1="449.58" y1="165.1" x2="449.58" y2="193.04" width="0.1524" layer="91"/>
<wire x1="449.58" y1="193.04" x2="467.36" y2="193.04" width="0.1524" layer="91"/>
<junction x="449.58" y="165.1"/>
<pinref part="LED20" gate="G$1" pin="A"/>
<wire x1="449.58" y1="193.04" x2="449.58" y2="220.98" width="0.1524" layer="91"/>
<wire x1="449.58" y1="220.98" x2="467.36" y2="220.98" width="0.1524" layer="91"/>
<junction x="449.58" y="193.04"/>
<pinref part="LED19" gate="G$1" pin="A"/>
<wire x1="449.58" y1="220.98" x2="449.58" y2="248.92" width="0.1524" layer="91"/>
<wire x1="449.58" y1="248.92" x2="467.36" y2="248.92" width="0.1524" layer="91"/>
<junction x="449.58" y="220.98"/>
</segment>
</net>
<net name="N$32" class="0">
<segment>
<pinref part="LED19" gate="G$1" pin="DP"/>
<wire x1="487.68" y1="233.68" x2="492.76" y2="233.68" width="0.1524" layer="91"/>
<wire x1="492.76" y1="233.68" x2="492.76" y2="205.74" width="0.1524" layer="91"/>
<pinref part="LED20" gate="G$1" pin="DP"/>
<wire x1="492.76" y1="205.74" x2="487.68" y2="205.74" width="0.1524" layer="91"/>
<pinref part="LED21" gate="G$1" pin="DP"/>
<wire x1="492.76" y1="205.74" x2="492.76" y2="177.8" width="0.1524" layer="91"/>
<wire x1="492.76" y1="177.8" x2="487.68" y2="177.8" width="0.1524" layer="91"/>
<junction x="492.76" y="205.74"/>
<pinref part="LED22" gate="G$1" pin="DP"/>
<wire x1="492.76" y1="177.8" x2="492.76" y2="149.86" width="0.1524" layer="91"/>
<wire x1="492.76" y1="149.86" x2="487.68" y2="149.86" width="0.1524" layer="91"/>
<junction x="492.76" y="177.8"/>
<pinref part="LED23" gate="G$1" pin="DP"/>
<wire x1="492.76" y1="149.86" x2="492.76" y2="121.92" width="0.1524" layer="91"/>
<wire x1="492.76" y1="121.92" x2="487.68" y2="121.92" width="0.1524" layer="91"/>
<junction x="492.76" y="149.86"/>
<pinref part="LED24" gate="G$1" pin="DP"/>
<wire x1="492.76" y1="121.92" x2="492.76" y2="91.44" width="0.1524" layer="91"/>
<wire x1="492.76" y1="91.44" x2="487.68" y2="91.44" width="0.1524" layer="91"/>
<junction x="492.76" y="121.92"/>
</segment>
</net>
<net name="N$49" class="0">
<segment>
<pinref part="LED37" gate="G$1" pin="A"/>
<wire x1="551.18" y1="248.92" x2="533.4" y2="248.92" width="0.1524" layer="91"/>
<wire x1="533.4" y1="248.92" x2="533.4" y2="220.98" width="0.1524" layer="91"/>
<pinref part="LED38" gate="G$1" pin="A"/>
<wire x1="533.4" y1="220.98" x2="551.18" y2="220.98" width="0.1524" layer="91"/>
<wire x1="533.4" y1="220.98" x2="533.4" y2="193.04" width="0.1524" layer="91"/>
<junction x="533.4" y="220.98"/>
<pinref part="LED39" gate="G$1" pin="A"/>
<wire x1="533.4" y1="193.04" x2="551.18" y2="193.04" width="0.1524" layer="91"/>
<wire x1="533.4" y1="193.04" x2="533.4" y2="165.1" width="0.1524" layer="91"/>
<junction x="533.4" y="193.04"/>
<pinref part="LED40" gate="G$1" pin="A"/>
<wire x1="533.4" y1="165.1" x2="551.18" y2="165.1" width="0.1524" layer="91"/>
<wire x1="533.4" y1="165.1" x2="533.4" y2="137.16" width="0.1524" layer="91"/>
<junction x="533.4" y="165.1"/>
<pinref part="LED41" gate="G$1" pin="A"/>
<wire x1="533.4" y1="137.16" x2="551.18" y2="137.16" width="0.1524" layer="91"/>
<wire x1="533.4" y1="137.16" x2="533.4" y2="106.68" width="0.1524" layer="91"/>
<junction x="533.4" y="137.16"/>
<pinref part="LED42" gate="G$1" pin="A"/>
<wire x1="533.4" y1="106.68" x2="551.18" y2="106.68" width="0.1524" layer="91"/>
</segment>
</net>
<net name="N$50" class="0">
<segment>
<pinref part="LED37" gate="G$1" pin="B"/>
<wire x1="551.18" y1="246.38" x2="535.94" y2="246.38" width="0.1524" layer="91"/>
<wire x1="535.94" y1="246.38" x2="535.94" y2="218.44" width="0.1524" layer="91"/>
<pinref part="LED38" gate="G$1" pin="B"/>
<wire x1="535.94" y1="218.44" x2="551.18" y2="218.44" width="0.1524" layer="91"/>
<wire x1="535.94" y1="218.44" x2="535.94" y2="190.5" width="0.1524" layer="91"/>
<junction x="535.94" y="218.44"/>
<pinref part="LED39" gate="G$1" pin="B"/>
<wire x1="535.94" y1="190.5" x2="551.18" y2="190.5" width="0.1524" layer="91"/>
<wire x1="535.94" y1="190.5" x2="535.94" y2="162.56" width="0.1524" layer="91"/>
<junction x="535.94" y="190.5"/>
<pinref part="LED40" gate="G$1" pin="B"/>
<wire x1="535.94" y1="162.56" x2="551.18" y2="162.56" width="0.1524" layer="91"/>
<wire x1="535.94" y1="162.56" x2="535.94" y2="134.62" width="0.1524" layer="91"/>
<junction x="535.94" y="162.56"/>
<pinref part="LED42" gate="G$1" pin="B"/>
<wire x1="535.94" y1="134.62" x2="535.94" y2="132.08" width="0.1524" layer="91"/>
<wire x1="535.94" y1="132.08" x2="535.94" y2="104.14" width="0.1524" layer="91"/>
<wire x1="535.94" y1="104.14" x2="551.18" y2="104.14" width="0.1524" layer="91"/>
<pinref part="LED41" gate="G$1" pin="B"/>
<wire x1="551.18" y1="134.62" x2="535.94" y2="134.62" width="0.1524" layer="91"/>
<junction x="535.94" y="134.62"/>
</segment>
</net>
<net name="N$51" class="0">
<segment>
<pinref part="LED42" gate="G$1" pin="G"/>
<wire x1="551.18" y1="91.44" x2="548.64" y2="91.44" width="0.1524" layer="91"/>
<wire x1="548.64" y1="91.44" x2="548.64" y2="121.92" width="0.1524" layer="91"/>
<pinref part="LED41" gate="G$1" pin="G"/>
<wire x1="548.64" y1="121.92" x2="551.18" y2="121.92" width="0.1524" layer="91"/>
<wire x1="548.64" y1="121.92" x2="548.64" y2="149.86" width="0.1524" layer="91"/>
<junction x="548.64" y="121.92"/>
<pinref part="LED40" gate="G$1" pin="G"/>
<wire x1="548.64" y1="149.86" x2="551.18" y2="149.86" width="0.1524" layer="91"/>
<wire x1="548.64" y1="149.86" x2="548.64" y2="177.8" width="0.1524" layer="91"/>
<junction x="548.64" y="149.86"/>
<pinref part="LED39" gate="G$1" pin="G"/>
<wire x1="548.64" y1="177.8" x2="551.18" y2="177.8" width="0.1524" layer="91"/>
<pinref part="LED38" gate="G$1" pin="G"/>
<wire x1="548.64" y1="177.8" x2="548.64" y2="205.74" width="0.1524" layer="91"/>
<wire x1="548.64" y1="205.74" x2="551.18" y2="205.74" width="0.1524" layer="91"/>
<junction x="548.64" y="177.8"/>
<pinref part="LED37" gate="G$1" pin="G"/>
<wire x1="548.64" y1="205.74" x2="548.64" y2="233.68" width="0.1524" layer="91"/>
<wire x1="548.64" y1="233.68" x2="551.18" y2="233.68" width="0.1524" layer="91"/>
<junction x="548.64" y="205.74"/>
</segment>
</net>
<net name="N$52" class="0">
<segment>
<pinref part="LED42" gate="G$1" pin="F"/>
<wire x1="551.18" y1="93.98" x2="546.1" y2="93.98" width="0.1524" layer="91"/>
<wire x1="546.1" y1="93.98" x2="546.1" y2="124.46" width="0.1524" layer="91"/>
<pinref part="LED41" gate="G$1" pin="F"/>
<wire x1="546.1" y1="124.46" x2="551.18" y2="124.46" width="0.1524" layer="91"/>
<wire x1="546.1" y1="124.46" x2="546.1" y2="152.4" width="0.1524" layer="91"/>
<junction x="546.1" y="124.46"/>
<pinref part="LED40" gate="G$1" pin="F"/>
<wire x1="546.1" y1="152.4" x2="551.18" y2="152.4" width="0.1524" layer="91"/>
<wire x1="546.1" y1="152.4" x2="546.1" y2="180.34" width="0.1524" layer="91"/>
<junction x="546.1" y="152.4"/>
<pinref part="LED39" gate="G$1" pin="F"/>
<wire x1="546.1" y1="180.34" x2="551.18" y2="180.34" width="0.1524" layer="91"/>
<wire x1="546.1" y1="180.34" x2="546.1" y2="208.28" width="0.1524" layer="91"/>
<junction x="546.1" y="180.34"/>
<pinref part="LED38" gate="G$1" pin="F"/>
<wire x1="546.1" y1="208.28" x2="551.18" y2="208.28" width="0.1524" layer="91"/>
<wire x1="546.1" y1="208.28" x2="546.1" y2="236.22" width="0.1524" layer="91"/>
<junction x="546.1" y="208.28"/>
<pinref part="LED37" gate="G$1" pin="F"/>
<wire x1="546.1" y1="236.22" x2="551.18" y2="236.22" width="0.1524" layer="91"/>
</segment>
</net>
<net name="N$53" class="0">
<segment>
<pinref part="LED42" gate="G$1" pin="E"/>
<wire x1="551.18" y1="96.52" x2="543.56" y2="96.52" width="0.1524" layer="91"/>
<wire x1="543.56" y1="96.52" x2="543.56" y2="127" width="0.1524" layer="91"/>
<pinref part="LED41" gate="G$1" pin="E"/>
<wire x1="543.56" y1="127" x2="551.18" y2="127" width="0.1524" layer="91"/>
<wire x1="543.56" y1="127" x2="543.56" y2="154.94" width="0.1524" layer="91"/>
<junction x="543.56" y="127"/>
<pinref part="LED40" gate="G$1" pin="E"/>
<wire x1="543.56" y1="154.94" x2="551.18" y2="154.94" width="0.1524" layer="91"/>
<wire x1="543.56" y1="154.94" x2="543.56" y2="182.88" width="0.1524" layer="91"/>
<junction x="543.56" y="154.94"/>
<pinref part="LED39" gate="G$1" pin="E"/>
<wire x1="543.56" y1="182.88" x2="551.18" y2="182.88" width="0.1524" layer="91"/>
<pinref part="LED38" gate="G$1" pin="E"/>
<wire x1="551.18" y1="210.82" x2="543.56" y2="210.82" width="0.1524" layer="91"/>
<wire x1="543.56" y1="210.82" x2="543.56" y2="182.88" width="0.1524" layer="91"/>
<junction x="543.56" y="182.88"/>
<pinref part="LED37" gate="G$1" pin="E"/>
<wire x1="551.18" y1="238.76" x2="543.56" y2="238.76" width="0.1524" layer="91"/>
<wire x1="543.56" y1="238.76" x2="543.56" y2="210.82" width="0.1524" layer="91"/>
<junction x="543.56" y="210.82"/>
</segment>
</net>
<net name="N$54" class="0">
<segment>
<pinref part="LED37" gate="G$1" pin="D"/>
<wire x1="551.18" y1="241.3" x2="541.02" y2="241.3" width="0.1524" layer="91"/>
<wire x1="541.02" y1="241.3" x2="541.02" y2="213.36" width="0.1524" layer="91"/>
<pinref part="LED38" gate="G$1" pin="D"/>
<wire x1="541.02" y1="213.36" x2="551.18" y2="213.36" width="0.1524" layer="91"/>
<pinref part="LED39" gate="G$1" pin="D"/>
<wire x1="541.02" y1="213.36" x2="541.02" y2="185.42" width="0.1524" layer="91"/>
<wire x1="541.02" y1="185.42" x2="551.18" y2="185.42" width="0.1524" layer="91"/>
<junction x="541.02" y="213.36"/>
<pinref part="LED40" gate="G$1" pin="D"/>
<wire x1="541.02" y1="185.42" x2="541.02" y2="157.48" width="0.1524" layer="91"/>
<wire x1="541.02" y1="157.48" x2="551.18" y2="157.48" width="0.1524" layer="91"/>
<junction x="541.02" y="185.42"/>
<pinref part="LED41" gate="G$1" pin="D"/>
<wire x1="541.02" y1="157.48" x2="541.02" y2="129.54" width="0.1524" layer="91"/>
<wire x1="541.02" y1="129.54" x2="551.18" y2="129.54" width="0.1524" layer="91"/>
<junction x="541.02" y="157.48"/>
<pinref part="LED42" gate="G$1" pin="D"/>
<wire x1="541.02" y1="129.54" x2="541.02" y2="99.06" width="0.1524" layer="91"/>
<wire x1="541.02" y1="99.06" x2="551.18" y2="99.06" width="0.1524" layer="91"/>
<junction x="541.02" y="129.54"/>
</segment>
</net>
<net name="N$55" class="0">
<segment>
<pinref part="LED37" gate="G$1" pin="C"/>
<wire x1="551.18" y1="243.84" x2="538.48" y2="243.84" width="0.1524" layer="91"/>
<pinref part="LED38" gate="G$1" pin="C"/>
<wire x1="538.48" y1="243.84" x2="538.48" y2="215.9" width="0.1524" layer="91"/>
<wire x1="538.48" y1="215.9" x2="551.18" y2="215.9" width="0.1524" layer="91"/>
<pinref part="LED39" gate="G$1" pin="C"/>
<wire x1="538.48" y1="215.9" x2="538.48" y2="187.96" width="0.1524" layer="91"/>
<wire x1="538.48" y1="187.96" x2="551.18" y2="187.96" width="0.1524" layer="91"/>
<junction x="538.48" y="215.9"/>
<pinref part="LED40" gate="G$1" pin="C"/>
<wire x1="538.48" y1="187.96" x2="538.48" y2="160.02" width="0.1524" layer="91"/>
<wire x1="538.48" y1="160.02" x2="551.18" y2="160.02" width="0.1524" layer="91"/>
<junction x="538.48" y="187.96"/>
<pinref part="LED41" gate="G$1" pin="C"/>
<wire x1="538.48" y1="160.02" x2="538.48" y2="132.08" width="0.1524" layer="91"/>
<wire x1="538.48" y1="132.08" x2="551.18" y2="132.08" width="0.1524" layer="91"/>
<junction x="538.48" y="160.02"/>
<pinref part="LED42" gate="G$1" pin="C"/>
<wire x1="538.48" y1="132.08" x2="538.48" y2="101.6" width="0.1524" layer="91"/>
<wire x1="538.48" y1="101.6" x2="551.18" y2="101.6" width="0.1524" layer="91"/>
<junction x="538.48" y="132.08"/>
</segment>
</net>
<net name="N$56" class="0">
<segment>
<pinref part="LED37" gate="G$1" pin="DP"/>
<wire x1="571.5" y1="233.68" x2="576.58" y2="233.68" width="0.1524" layer="91"/>
<wire x1="576.58" y1="233.68" x2="576.58" y2="205.74" width="0.1524" layer="91"/>
<pinref part="LED38" gate="G$1" pin="DP"/>
<wire x1="576.58" y1="205.74" x2="571.5" y2="205.74" width="0.1524" layer="91"/>
<pinref part="LED39" gate="G$1" pin="DP"/>
<wire x1="576.58" y1="205.74" x2="576.58" y2="177.8" width="0.1524" layer="91"/>
<wire x1="576.58" y1="177.8" x2="571.5" y2="177.8" width="0.1524" layer="91"/>
<junction x="576.58" y="205.74"/>
<pinref part="LED40" gate="G$1" pin="DP"/>
<wire x1="576.58" y1="177.8" x2="576.58" y2="149.86" width="0.1524" layer="91"/>
<wire x1="576.58" y1="149.86" x2="571.5" y2="149.86" width="0.1524" layer="91"/>
<junction x="576.58" y="177.8"/>
<pinref part="LED41" gate="G$1" pin="DP"/>
<wire x1="576.58" y1="149.86" x2="576.58" y2="121.92" width="0.1524" layer="91"/>
<wire x1="576.58" y1="121.92" x2="571.5" y2="121.92" width="0.1524" layer="91"/>
<junction x="576.58" y="149.86"/>
<pinref part="LED42" gate="G$1" pin="DP"/>
<wire x1="576.58" y1="121.92" x2="576.58" y2="91.44" width="0.1524" layer="91"/>
<wire x1="576.58" y1="91.44" x2="571.5" y2="91.44" width="0.1524" layer="91"/>
<junction x="576.58" y="121.92"/>
</segment>
</net>
<net name="N$57" class="0">
<segment>
<pinref part="LED48" gate="G$1" pin="G"/>
<wire x1="609.6" y1="91.44" x2="607.06" y2="91.44" width="0.1524" layer="91"/>
<wire x1="607.06" y1="91.44" x2="607.06" y2="121.92" width="0.1524" layer="91"/>
<pinref part="LED47" gate="G$1" pin="G"/>
<wire x1="609.6" y1="121.92" x2="607.06" y2="121.92" width="0.1524" layer="91"/>
<pinref part="LED46" gate="G$1" pin="G"/>
<wire x1="607.06" y1="121.92" x2="607.06" y2="149.86" width="0.1524" layer="91"/>
<wire x1="607.06" y1="149.86" x2="609.6" y2="149.86" width="0.1524" layer="91"/>
<junction x="607.06" y="121.92"/>
<wire x1="607.06" y1="149.86" x2="607.06" y2="177.8" width="0.1524" layer="91"/>
<junction x="607.06" y="149.86"/>
<pinref part="LED45" gate="G$1" pin="G"/>
<wire x1="607.06" y1="177.8" x2="609.6" y2="177.8" width="0.1524" layer="91"/>
<pinref part="LED44" gate="G$1" pin="G"/>
<wire x1="607.06" y1="177.8" x2="607.06" y2="205.74" width="0.1524" layer="91"/>
<wire x1="607.06" y1="205.74" x2="609.6" y2="205.74" width="0.1524" layer="91"/>
<junction x="607.06" y="177.8"/>
<pinref part="LED43" gate="G$1" pin="G"/>
<wire x1="607.06" y1="205.74" x2="607.06" y2="233.68" width="0.1524" layer="91"/>
<wire x1="607.06" y1="233.68" x2="609.6" y2="233.68" width="0.1524" layer="91"/>
<junction x="607.06" y="205.74"/>
</segment>
</net>
<net name="N$58" class="0">
<segment>
<pinref part="LED48" gate="G$1" pin="F"/>
<wire x1="609.6" y1="93.98" x2="604.52" y2="93.98" width="0.1524" layer="91"/>
<pinref part="LED47" gate="G$1" pin="F"/>
<wire x1="604.52" y1="93.98" x2="604.52" y2="124.46" width="0.1524" layer="91"/>
<wire x1="604.52" y1="124.46" x2="609.6" y2="124.46" width="0.1524" layer="91"/>
<pinref part="LED46" gate="G$1" pin="F"/>
<wire x1="604.52" y1="124.46" x2="604.52" y2="152.4" width="0.1524" layer="91"/>
<wire x1="604.52" y1="152.4" x2="609.6" y2="152.4" width="0.1524" layer="91"/>
<junction x="604.52" y="124.46"/>
<pinref part="LED45" gate="G$1" pin="F"/>
<wire x1="604.52" y1="152.4" x2="604.52" y2="180.34" width="0.1524" layer="91"/>
<wire x1="604.52" y1="180.34" x2="609.6" y2="180.34" width="0.1524" layer="91"/>
<junction x="604.52" y="152.4"/>
<pinref part="LED44" gate="G$1" pin="F"/>
<wire x1="604.52" y1="180.34" x2="604.52" y2="208.28" width="0.1524" layer="91"/>
<wire x1="604.52" y1="208.28" x2="609.6" y2="208.28" width="0.1524" layer="91"/>
<junction x="604.52" y="180.34"/>
<pinref part="LED43" gate="G$1" pin="F"/>
<wire x1="604.52" y1="208.28" x2="604.52" y2="236.22" width="0.1524" layer="91"/>
<wire x1="604.52" y1="236.22" x2="609.6" y2="236.22" width="0.1524" layer="91"/>
<junction x="604.52" y="208.28"/>
</segment>
</net>
<net name="N$59" class="0">
<segment>
<pinref part="LED48" gate="G$1" pin="E"/>
<wire x1="609.6" y1="96.52" x2="601.98" y2="96.52" width="0.1524" layer="91"/>
<pinref part="LED47" gate="G$1" pin="E"/>
<wire x1="601.98" y1="96.52" x2="601.98" y2="127" width="0.1524" layer="91"/>
<wire x1="601.98" y1="127" x2="609.6" y2="127" width="0.1524" layer="91"/>
<pinref part="LED46" gate="G$1" pin="E"/>
<wire x1="601.98" y1="127" x2="601.98" y2="154.94" width="0.1524" layer="91"/>
<wire x1="601.98" y1="154.94" x2="609.6" y2="154.94" width="0.1524" layer="91"/>
<junction x="601.98" y="127"/>
<pinref part="LED45" gate="G$1" pin="E"/>
<wire x1="601.98" y1="154.94" x2="601.98" y2="182.88" width="0.1524" layer="91"/>
<wire x1="601.98" y1="182.88" x2="609.6" y2="182.88" width="0.1524" layer="91"/>
<junction x="601.98" y="154.94"/>
<pinref part="LED44" gate="G$1" pin="E"/>
<wire x1="601.98" y1="182.88" x2="601.98" y2="210.82" width="0.1524" layer="91"/>
<wire x1="601.98" y1="210.82" x2="609.6" y2="210.82" width="0.1524" layer="91"/>
<junction x="601.98" y="182.88"/>
<pinref part="LED43" gate="G$1" pin="E"/>
<wire x1="601.98" y1="210.82" x2="601.98" y2="238.76" width="0.1524" layer="91"/>
<wire x1="601.98" y1="238.76" x2="609.6" y2="238.76" width="0.1524" layer="91"/>
<junction x="601.98" y="210.82"/>
</segment>
</net>
<net name="N$60" class="0">
<segment>
<pinref part="LED48" gate="G$1" pin="D"/>
<wire x1="609.6" y1="99.06" x2="599.44" y2="99.06" width="0.1524" layer="91"/>
<pinref part="LED47" gate="G$1" pin="D"/>
<wire x1="599.44" y1="99.06" x2="599.44" y2="129.54" width="0.1524" layer="91"/>
<wire x1="599.44" y1="129.54" x2="609.6" y2="129.54" width="0.1524" layer="91"/>
<pinref part="LED46" gate="G$1" pin="D"/>
<wire x1="599.44" y1="129.54" x2="599.44" y2="157.48" width="0.1524" layer="91"/>
<wire x1="599.44" y1="157.48" x2="609.6" y2="157.48" width="0.1524" layer="91"/>
<junction x="599.44" y="129.54"/>
<pinref part="LED45" gate="G$1" pin="D"/>
<wire x1="599.44" y1="157.48" x2="599.44" y2="185.42" width="0.1524" layer="91"/>
<wire x1="599.44" y1="185.42" x2="609.6" y2="185.42" width="0.1524" layer="91"/>
<junction x="599.44" y="157.48"/>
<pinref part="LED44" gate="G$1" pin="D"/>
<wire x1="599.44" y1="185.42" x2="599.44" y2="213.36" width="0.1524" layer="91"/>
<wire x1="599.44" y1="213.36" x2="609.6" y2="213.36" width="0.1524" layer="91"/>
<junction x="599.44" y="185.42"/>
<pinref part="LED43" gate="G$1" pin="D"/>
<wire x1="599.44" y1="213.36" x2="599.44" y2="241.3" width="0.1524" layer="91"/>
<wire x1="599.44" y1="241.3" x2="609.6" y2="241.3" width="0.1524" layer="91"/>
<junction x="599.44" y="213.36"/>
</segment>
</net>
<net name="N$61" class="0">
<segment>
<pinref part="LED48" gate="G$1" pin="C"/>
<wire x1="609.6" y1="101.6" x2="596.9" y2="101.6" width="0.1524" layer="91"/>
<pinref part="LED47" gate="G$1" pin="C"/>
<wire x1="596.9" y1="101.6" x2="596.9" y2="132.08" width="0.1524" layer="91"/>
<wire x1="596.9" y1="132.08" x2="609.6" y2="132.08" width="0.1524" layer="91"/>
<pinref part="LED46" gate="G$1" pin="C"/>
<wire x1="596.9" y1="132.08" x2="596.9" y2="160.02" width="0.1524" layer="91"/>
<wire x1="596.9" y1="160.02" x2="609.6" y2="160.02" width="0.1524" layer="91"/>
<junction x="596.9" y="132.08"/>
<pinref part="LED45" gate="G$1" pin="C"/>
<wire x1="596.9" y1="160.02" x2="596.9" y2="187.96" width="0.1524" layer="91"/>
<wire x1="596.9" y1="187.96" x2="609.6" y2="187.96" width="0.1524" layer="91"/>
<junction x="596.9" y="160.02"/>
<pinref part="LED44" gate="G$1" pin="C"/>
<wire x1="596.9" y1="187.96" x2="596.9" y2="215.9" width="0.1524" layer="91"/>
<wire x1="596.9" y1="215.9" x2="609.6" y2="215.9" width="0.1524" layer="91"/>
<junction x="596.9" y="187.96"/>
<pinref part="LED43" gate="G$1" pin="C"/>
<wire x1="596.9" y1="215.9" x2="596.9" y2="243.84" width="0.1524" layer="91"/>
<wire x1="596.9" y1="243.84" x2="609.6" y2="243.84" width="0.1524" layer="91"/>
<junction x="596.9" y="215.9"/>
</segment>
</net>
<net name="N$62" class="0">
<segment>
<pinref part="LED48" gate="G$1" pin="B"/>
<wire x1="609.6" y1="104.14" x2="594.36" y2="104.14" width="0.1524" layer="91"/>
<pinref part="LED47" gate="G$1" pin="B"/>
<wire x1="594.36" y1="104.14" x2="594.36" y2="134.62" width="0.1524" layer="91"/>
<wire x1="594.36" y1="134.62" x2="609.6" y2="134.62" width="0.1524" layer="91"/>
<pinref part="LED46" gate="G$1" pin="B"/>
<wire x1="594.36" y1="134.62" x2="594.36" y2="162.56" width="0.1524" layer="91"/>
<wire x1="594.36" y1="162.56" x2="609.6" y2="162.56" width="0.1524" layer="91"/>
<junction x="594.36" y="134.62"/>
<pinref part="LED45" gate="G$1" pin="B"/>
<wire x1="594.36" y1="162.56" x2="594.36" y2="190.5" width="0.1524" layer="91"/>
<wire x1="594.36" y1="190.5" x2="609.6" y2="190.5" width="0.1524" layer="91"/>
<junction x="594.36" y="162.56"/>
<pinref part="LED44" gate="G$1" pin="B"/>
<wire x1="594.36" y1="190.5" x2="594.36" y2="218.44" width="0.1524" layer="91"/>
<wire x1="594.36" y1="218.44" x2="609.6" y2="218.44" width="0.1524" layer="91"/>
<junction x="594.36" y="190.5"/>
<pinref part="LED43" gate="G$1" pin="B"/>
<wire x1="594.36" y1="218.44" x2="594.36" y2="246.38" width="0.1524" layer="91"/>
<wire x1="594.36" y1="246.38" x2="609.6" y2="246.38" width="0.1524" layer="91"/>
<junction x="594.36" y="218.44"/>
</segment>
</net>
<net name="N$63" class="0">
<segment>
<pinref part="LED48" gate="G$1" pin="A"/>
<wire x1="609.6" y1="106.68" x2="591.82" y2="106.68" width="0.1524" layer="91"/>
<pinref part="LED47" gate="G$1" pin="A"/>
<wire x1="591.82" y1="106.68" x2="591.82" y2="137.16" width="0.1524" layer="91"/>
<wire x1="591.82" y1="137.16" x2="609.6" y2="137.16" width="0.1524" layer="91"/>
<pinref part="LED46" gate="G$1" pin="A"/>
<wire x1="591.82" y1="137.16" x2="591.82" y2="165.1" width="0.1524" layer="91"/>
<wire x1="591.82" y1="165.1" x2="609.6" y2="165.1" width="0.1524" layer="91"/>
<junction x="591.82" y="137.16"/>
<pinref part="LED45" gate="G$1" pin="A"/>
<wire x1="591.82" y1="165.1" x2="591.82" y2="193.04" width="0.1524" layer="91"/>
<wire x1="591.82" y1="193.04" x2="609.6" y2="193.04" width="0.1524" layer="91"/>
<junction x="591.82" y="165.1"/>
<pinref part="LED44" gate="G$1" pin="A"/>
<wire x1="591.82" y1="193.04" x2="591.82" y2="220.98" width="0.1524" layer="91"/>
<wire x1="591.82" y1="220.98" x2="609.6" y2="220.98" width="0.1524" layer="91"/>
<junction x="591.82" y="193.04"/>
<pinref part="LED43" gate="G$1" pin="A"/>
<wire x1="591.82" y1="220.98" x2="591.82" y2="248.92" width="0.1524" layer="91"/>
<wire x1="591.82" y1="248.92" x2="609.6" y2="248.92" width="0.1524" layer="91"/>
<junction x="591.82" y="220.98"/>
</segment>
</net>
<net name="N$64" class="0">
<segment>
<pinref part="LED43" gate="G$1" pin="DP"/>
<wire x1="629.92" y1="233.68" x2="635" y2="233.68" width="0.1524" layer="91"/>
<wire x1="635" y1="233.68" x2="635" y2="205.74" width="0.1524" layer="91"/>
<pinref part="LED44" gate="G$1" pin="DP"/>
<wire x1="635" y1="205.74" x2="629.92" y2="205.74" width="0.1524" layer="91"/>
<pinref part="LED45" gate="G$1" pin="DP"/>
<wire x1="635" y1="205.74" x2="635" y2="177.8" width="0.1524" layer="91"/>
<wire x1="635" y1="177.8" x2="629.92" y2="177.8" width="0.1524" layer="91"/>
<junction x="635" y="205.74"/>
<pinref part="LED46" gate="G$1" pin="DP"/>
<wire x1="635" y1="177.8" x2="635" y2="149.86" width="0.1524" layer="91"/>
<wire x1="635" y1="149.86" x2="629.92" y2="149.86" width="0.1524" layer="91"/>
<junction x="635" y="177.8"/>
<pinref part="LED47" gate="G$1" pin="DP"/>
<wire x1="635" y1="149.86" x2="635" y2="121.92" width="0.1524" layer="91"/>
<wire x1="635" y1="121.92" x2="629.92" y2="121.92" width="0.1524" layer="91"/>
<junction x="635" y="149.86"/>
<pinref part="LED48" gate="G$1" pin="DP"/>
<wire x1="635" y1="121.92" x2="635" y2="91.44" width="0.1524" layer="91"/>
<wire x1="635" y1="91.44" x2="629.92" y2="91.44" width="0.1524" layer="91"/>
<junction x="635" y="121.92"/>
</segment>
</net>
<net name="N$33" class="0">
<segment>
<pinref part="LED25" gate="G$1" pin="A"/>
<wire x1="701.04" y1="248.92" x2="683.26" y2="248.92" width="0.1524" layer="91"/>
<wire x1="683.26" y1="248.92" x2="683.26" y2="220.98" width="0.1524" layer="91"/>
<pinref part="LED26" gate="G$1" pin="A"/>
<wire x1="683.26" y1="220.98" x2="701.04" y2="220.98" width="0.1524" layer="91"/>
<wire x1="683.26" y1="220.98" x2="683.26" y2="193.04" width="0.1524" layer="91"/>
<junction x="683.26" y="220.98"/>
<pinref part="LED27" gate="G$1" pin="A"/>
<wire x1="683.26" y1="193.04" x2="701.04" y2="193.04" width="0.1524" layer="91"/>
<wire x1="683.26" y1="193.04" x2="683.26" y2="165.1" width="0.1524" layer="91"/>
<junction x="683.26" y="193.04"/>
<pinref part="LED28" gate="G$1" pin="A"/>
<wire x1="683.26" y1="165.1" x2="701.04" y2="165.1" width="0.1524" layer="91"/>
<wire x1="683.26" y1="165.1" x2="683.26" y2="137.16" width="0.1524" layer="91"/>
<junction x="683.26" y="165.1"/>
<pinref part="LED29" gate="G$1" pin="A"/>
<wire x1="683.26" y1="137.16" x2="701.04" y2="137.16" width="0.1524" layer="91"/>
<wire x1="683.26" y1="137.16" x2="683.26" y2="106.68" width="0.1524" layer="91"/>
<junction x="683.26" y="137.16"/>
<pinref part="LED30" gate="G$1" pin="A"/>
<wire x1="683.26" y1="106.68" x2="701.04" y2="106.68" width="0.1524" layer="91"/>
</segment>
</net>
<net name="N$34" class="0">
<segment>
<pinref part="LED25" gate="G$1" pin="B"/>
<wire x1="701.04" y1="246.38" x2="685.8" y2="246.38" width="0.1524" layer="91"/>
<wire x1="685.8" y1="246.38" x2="685.8" y2="218.44" width="0.1524" layer="91"/>
<pinref part="LED26" gate="G$1" pin="B"/>
<wire x1="685.8" y1="218.44" x2="701.04" y2="218.44" width="0.1524" layer="91"/>
<wire x1="685.8" y1="218.44" x2="685.8" y2="190.5" width="0.1524" layer="91"/>
<junction x="685.8" y="218.44"/>
<pinref part="LED27" gate="G$1" pin="B"/>
<wire x1="685.8" y1="190.5" x2="701.04" y2="190.5" width="0.1524" layer="91"/>
<wire x1="685.8" y1="190.5" x2="685.8" y2="162.56" width="0.1524" layer="91"/>
<junction x="685.8" y="190.5"/>
<pinref part="LED28" gate="G$1" pin="B"/>
<wire x1="685.8" y1="162.56" x2="701.04" y2="162.56" width="0.1524" layer="91"/>
<wire x1="685.8" y1="162.56" x2="685.8" y2="134.62" width="0.1524" layer="91"/>
<junction x="685.8" y="162.56"/>
<pinref part="LED30" gate="G$1" pin="B"/>
<wire x1="685.8" y1="134.62" x2="685.8" y2="132.08" width="0.1524" layer="91"/>
<wire x1="685.8" y1="132.08" x2="685.8" y2="104.14" width="0.1524" layer="91"/>
<wire x1="685.8" y1="104.14" x2="701.04" y2="104.14" width="0.1524" layer="91"/>
<pinref part="LED29" gate="G$1" pin="B"/>
<wire x1="701.04" y1="134.62" x2="685.8" y2="134.62" width="0.1524" layer="91"/>
<junction x="685.8" y="134.62"/>
</segment>
</net>
<net name="N$35" class="0">
<segment>
<pinref part="LED30" gate="G$1" pin="G"/>
<wire x1="701.04" y1="91.44" x2="698.5" y2="91.44" width="0.1524" layer="91"/>
<wire x1="698.5" y1="91.44" x2="698.5" y2="121.92" width="0.1524" layer="91"/>
<pinref part="LED29" gate="G$1" pin="G"/>
<wire x1="698.5" y1="121.92" x2="701.04" y2="121.92" width="0.1524" layer="91"/>
<wire x1="698.5" y1="121.92" x2="698.5" y2="149.86" width="0.1524" layer="91"/>
<junction x="698.5" y="121.92"/>
<pinref part="LED28" gate="G$1" pin="G"/>
<wire x1="698.5" y1="149.86" x2="701.04" y2="149.86" width="0.1524" layer="91"/>
<wire x1="698.5" y1="149.86" x2="698.5" y2="177.8" width="0.1524" layer="91"/>
<junction x="698.5" y="149.86"/>
<pinref part="LED27" gate="G$1" pin="G"/>
<wire x1="698.5" y1="177.8" x2="701.04" y2="177.8" width="0.1524" layer="91"/>
<pinref part="LED26" gate="G$1" pin="G"/>
<wire x1="698.5" y1="177.8" x2="698.5" y2="205.74" width="0.1524" layer="91"/>
<wire x1="698.5" y1="205.74" x2="701.04" y2="205.74" width="0.1524" layer="91"/>
<junction x="698.5" y="177.8"/>
<pinref part="LED25" gate="G$1" pin="G"/>
<wire x1="698.5" y1="205.74" x2="698.5" y2="233.68" width="0.1524" layer="91"/>
<wire x1="698.5" y1="233.68" x2="701.04" y2="233.68" width="0.1524" layer="91"/>
<junction x="698.5" y="205.74"/>
</segment>
</net>
<net name="N$36" class="0">
<segment>
<pinref part="LED30" gate="G$1" pin="F"/>
<wire x1="701.04" y1="93.98" x2="695.96" y2="93.98" width="0.1524" layer="91"/>
<wire x1="695.96" y1="93.98" x2="695.96" y2="124.46" width="0.1524" layer="91"/>
<pinref part="LED29" gate="G$1" pin="F"/>
<wire x1="695.96" y1="124.46" x2="701.04" y2="124.46" width="0.1524" layer="91"/>
<wire x1="695.96" y1="124.46" x2="695.96" y2="152.4" width="0.1524" layer="91"/>
<junction x="695.96" y="124.46"/>
<pinref part="LED28" gate="G$1" pin="F"/>
<wire x1="695.96" y1="152.4" x2="701.04" y2="152.4" width="0.1524" layer="91"/>
<wire x1="695.96" y1="152.4" x2="695.96" y2="180.34" width="0.1524" layer="91"/>
<junction x="695.96" y="152.4"/>
<pinref part="LED27" gate="G$1" pin="F"/>
<wire x1="695.96" y1="180.34" x2="701.04" y2="180.34" width="0.1524" layer="91"/>
<wire x1="695.96" y1="180.34" x2="695.96" y2="208.28" width="0.1524" layer="91"/>
<junction x="695.96" y="180.34"/>
<pinref part="LED26" gate="G$1" pin="F"/>
<wire x1="695.96" y1="208.28" x2="701.04" y2="208.28" width="0.1524" layer="91"/>
<wire x1="695.96" y1="208.28" x2="695.96" y2="236.22" width="0.1524" layer="91"/>
<junction x="695.96" y="208.28"/>
<pinref part="LED25" gate="G$1" pin="F"/>
<wire x1="695.96" y1="236.22" x2="701.04" y2="236.22" width="0.1524" layer="91"/>
</segment>
</net>
<net name="N$37" class="0">
<segment>
<pinref part="LED30" gate="G$1" pin="E"/>
<wire x1="701.04" y1="96.52" x2="693.42" y2="96.52" width="0.1524" layer="91"/>
<wire x1="693.42" y1="96.52" x2="693.42" y2="127" width="0.1524" layer="91"/>
<pinref part="LED29" gate="G$1" pin="E"/>
<wire x1="693.42" y1="127" x2="701.04" y2="127" width="0.1524" layer="91"/>
<wire x1="693.42" y1="127" x2="693.42" y2="154.94" width="0.1524" layer="91"/>
<junction x="693.42" y="127"/>
<pinref part="LED28" gate="G$1" pin="E"/>
<wire x1="693.42" y1="154.94" x2="701.04" y2="154.94" width="0.1524" layer="91"/>
<wire x1="693.42" y1="154.94" x2="693.42" y2="182.88" width="0.1524" layer="91"/>
<junction x="693.42" y="154.94"/>
<pinref part="LED27" gate="G$1" pin="E"/>
<wire x1="693.42" y1="182.88" x2="701.04" y2="182.88" width="0.1524" layer="91"/>
<pinref part="LED26" gate="G$1" pin="E"/>
<wire x1="701.04" y1="210.82" x2="693.42" y2="210.82" width="0.1524" layer="91"/>
<wire x1="693.42" y1="210.82" x2="693.42" y2="182.88" width="0.1524" layer="91"/>
<junction x="693.42" y="182.88"/>
<pinref part="LED25" gate="G$1" pin="E"/>
<wire x1="701.04" y1="238.76" x2="693.42" y2="238.76" width="0.1524" layer="91"/>
<wire x1="693.42" y1="238.76" x2="693.42" y2="210.82" width="0.1524" layer="91"/>
<junction x="693.42" y="210.82"/>
</segment>
</net>
<net name="N$38" class="0">
<segment>
<pinref part="LED25" gate="G$1" pin="D"/>
<wire x1="701.04" y1="241.3" x2="690.88" y2="241.3" width="0.1524" layer="91"/>
<wire x1="690.88" y1="241.3" x2="690.88" y2="213.36" width="0.1524" layer="91"/>
<pinref part="LED26" gate="G$1" pin="D"/>
<wire x1="690.88" y1="213.36" x2="701.04" y2="213.36" width="0.1524" layer="91"/>
<pinref part="LED27" gate="G$1" pin="D"/>
<wire x1="690.88" y1="213.36" x2="690.88" y2="185.42" width="0.1524" layer="91"/>
<wire x1="690.88" y1="185.42" x2="701.04" y2="185.42" width="0.1524" layer="91"/>
<junction x="690.88" y="213.36"/>
<pinref part="LED28" gate="G$1" pin="D"/>
<wire x1="690.88" y1="185.42" x2="690.88" y2="157.48" width="0.1524" layer="91"/>
<wire x1="690.88" y1="157.48" x2="701.04" y2="157.48" width="0.1524" layer="91"/>
<junction x="690.88" y="185.42"/>
<pinref part="LED29" gate="G$1" pin="D"/>
<wire x1="690.88" y1="157.48" x2="690.88" y2="129.54" width="0.1524" layer="91"/>
<wire x1="690.88" y1="129.54" x2="701.04" y2="129.54" width="0.1524" layer="91"/>
<junction x="690.88" y="157.48"/>
<pinref part="LED30" gate="G$1" pin="D"/>
<wire x1="690.88" y1="129.54" x2="690.88" y2="99.06" width="0.1524" layer="91"/>
<wire x1="690.88" y1="99.06" x2="701.04" y2="99.06" width="0.1524" layer="91"/>
<junction x="690.88" y="129.54"/>
</segment>
</net>
<net name="N$39" class="0">
<segment>
<pinref part="LED25" gate="G$1" pin="C"/>
<wire x1="701.04" y1="243.84" x2="688.34" y2="243.84" width="0.1524" layer="91"/>
<pinref part="LED26" gate="G$1" pin="C"/>
<wire x1="688.34" y1="243.84" x2="688.34" y2="215.9" width="0.1524" layer="91"/>
<wire x1="688.34" y1="215.9" x2="701.04" y2="215.9" width="0.1524" layer="91"/>
<pinref part="LED27" gate="G$1" pin="C"/>
<wire x1="688.34" y1="215.9" x2="688.34" y2="187.96" width="0.1524" layer="91"/>
<wire x1="688.34" y1="187.96" x2="701.04" y2="187.96" width="0.1524" layer="91"/>
<junction x="688.34" y="215.9"/>
<pinref part="LED28" gate="G$1" pin="C"/>
<wire x1="688.34" y1="187.96" x2="688.34" y2="160.02" width="0.1524" layer="91"/>
<wire x1="688.34" y1="160.02" x2="701.04" y2="160.02" width="0.1524" layer="91"/>
<junction x="688.34" y="187.96"/>
<pinref part="LED29" gate="G$1" pin="C"/>
<wire x1="688.34" y1="160.02" x2="688.34" y2="132.08" width="0.1524" layer="91"/>
<wire x1="688.34" y1="132.08" x2="701.04" y2="132.08" width="0.1524" layer="91"/>
<junction x="688.34" y="160.02"/>
<pinref part="LED30" gate="G$1" pin="C"/>
<wire x1="688.34" y1="132.08" x2="688.34" y2="101.6" width="0.1524" layer="91"/>
<wire x1="688.34" y1="101.6" x2="701.04" y2="101.6" width="0.1524" layer="91"/>
<junction x="688.34" y="132.08"/>
</segment>
</net>
<net name="N$40" class="0">
<segment>
<pinref part="LED25" gate="G$1" pin="DP"/>
<wire x1="721.36" y1="233.68" x2="726.44" y2="233.68" width="0.1524" layer="91"/>
<wire x1="726.44" y1="233.68" x2="726.44" y2="205.74" width="0.1524" layer="91"/>
<pinref part="LED26" gate="G$1" pin="DP"/>
<wire x1="726.44" y1="205.74" x2="721.36" y2="205.74" width="0.1524" layer="91"/>
<pinref part="LED27" gate="G$1" pin="DP"/>
<wire x1="726.44" y1="205.74" x2="726.44" y2="177.8" width="0.1524" layer="91"/>
<wire x1="726.44" y1="177.8" x2="721.36" y2="177.8" width="0.1524" layer="91"/>
<junction x="726.44" y="205.74"/>
<pinref part="LED28" gate="G$1" pin="DP"/>
<wire x1="726.44" y1="177.8" x2="726.44" y2="149.86" width="0.1524" layer="91"/>
<wire x1="726.44" y1="149.86" x2="721.36" y2="149.86" width="0.1524" layer="91"/>
<junction x="726.44" y="177.8"/>
<pinref part="LED29" gate="G$1" pin="DP"/>
<wire x1="726.44" y1="149.86" x2="726.44" y2="121.92" width="0.1524" layer="91"/>
<wire x1="726.44" y1="121.92" x2="721.36" y2="121.92" width="0.1524" layer="91"/>
<junction x="726.44" y="149.86"/>
<pinref part="LED30" gate="G$1" pin="DP"/>
<wire x1="726.44" y1="121.92" x2="726.44" y2="91.44" width="0.1524" layer="91"/>
<wire x1="726.44" y1="91.44" x2="721.36" y2="91.44" width="0.1524" layer="91"/>
<junction x="726.44" y="121.92"/>
</segment>
</net>
<net name="N$41" class="0">
<segment>
<pinref part="LED36" gate="G$1" pin="G"/>
<wire x1="759.46" y1="91.44" x2="756.92" y2="91.44" width="0.1524" layer="91"/>
<wire x1="756.92" y1="91.44" x2="756.92" y2="121.92" width="0.1524" layer="91"/>
<pinref part="LED35" gate="G$1" pin="G"/>
<wire x1="759.46" y1="121.92" x2="756.92" y2="121.92" width="0.1524" layer="91"/>
<pinref part="LED34" gate="G$1" pin="G"/>
<wire x1="756.92" y1="121.92" x2="756.92" y2="149.86" width="0.1524" layer="91"/>
<wire x1="756.92" y1="149.86" x2="759.46" y2="149.86" width="0.1524" layer="91"/>
<junction x="756.92" y="121.92"/>
<wire x1="756.92" y1="149.86" x2="756.92" y2="177.8" width="0.1524" layer="91"/>
<junction x="756.92" y="149.86"/>
<pinref part="LED33" gate="G$1" pin="G"/>
<wire x1="756.92" y1="177.8" x2="759.46" y2="177.8" width="0.1524" layer="91"/>
<pinref part="LED32" gate="G$1" pin="G"/>
<wire x1="756.92" y1="177.8" x2="756.92" y2="205.74" width="0.1524" layer="91"/>
<wire x1="756.92" y1="205.74" x2="759.46" y2="205.74" width="0.1524" layer="91"/>
<junction x="756.92" y="177.8"/>
<pinref part="LED31" gate="G$1" pin="G"/>
<wire x1="756.92" y1="205.74" x2="756.92" y2="233.68" width="0.1524" layer="91"/>
<wire x1="756.92" y1="233.68" x2="759.46" y2="233.68" width="0.1524" layer="91"/>
<junction x="756.92" y="205.74"/>
</segment>
</net>
<net name="N$42" class="0">
<segment>
<pinref part="LED36" gate="G$1" pin="F"/>
<wire x1="759.46" y1="93.98" x2="754.38" y2="93.98" width="0.1524" layer="91"/>
<pinref part="LED35" gate="G$1" pin="F"/>
<wire x1="754.38" y1="93.98" x2="754.38" y2="124.46" width="0.1524" layer="91"/>
<wire x1="754.38" y1="124.46" x2="759.46" y2="124.46" width="0.1524" layer="91"/>
<pinref part="LED34" gate="G$1" pin="F"/>
<wire x1="754.38" y1="124.46" x2="754.38" y2="152.4" width="0.1524" layer="91"/>
<wire x1="754.38" y1="152.4" x2="759.46" y2="152.4" width="0.1524" layer="91"/>
<junction x="754.38" y="124.46"/>
<pinref part="LED33" gate="G$1" pin="F"/>
<wire x1="754.38" y1="152.4" x2="754.38" y2="180.34" width="0.1524" layer="91"/>
<wire x1="754.38" y1="180.34" x2="759.46" y2="180.34" width="0.1524" layer="91"/>
<junction x="754.38" y="152.4"/>
<pinref part="LED32" gate="G$1" pin="F"/>
<wire x1="754.38" y1="180.34" x2="754.38" y2="208.28" width="0.1524" layer="91"/>
<wire x1="754.38" y1="208.28" x2="759.46" y2="208.28" width="0.1524" layer="91"/>
<junction x="754.38" y="180.34"/>
<pinref part="LED31" gate="G$1" pin="F"/>
<wire x1="754.38" y1="208.28" x2="754.38" y2="236.22" width="0.1524" layer="91"/>
<wire x1="754.38" y1="236.22" x2="759.46" y2="236.22" width="0.1524" layer="91"/>
<junction x="754.38" y="208.28"/>
</segment>
</net>
<net name="N$43" class="0">
<segment>
<pinref part="LED36" gate="G$1" pin="E"/>
<wire x1="759.46" y1="96.52" x2="751.84" y2="96.52" width="0.1524" layer="91"/>
<pinref part="LED35" gate="G$1" pin="E"/>
<wire x1="751.84" y1="96.52" x2="751.84" y2="127" width="0.1524" layer="91"/>
<wire x1="751.84" y1="127" x2="759.46" y2="127" width="0.1524" layer="91"/>
<pinref part="LED34" gate="G$1" pin="E"/>
<wire x1="751.84" y1="127" x2="751.84" y2="154.94" width="0.1524" layer="91"/>
<wire x1="751.84" y1="154.94" x2="759.46" y2="154.94" width="0.1524" layer="91"/>
<junction x="751.84" y="127"/>
<pinref part="LED33" gate="G$1" pin="E"/>
<wire x1="751.84" y1="154.94" x2="751.84" y2="182.88" width="0.1524" layer="91"/>
<wire x1="751.84" y1="182.88" x2="759.46" y2="182.88" width="0.1524" layer="91"/>
<junction x="751.84" y="154.94"/>
<pinref part="LED32" gate="G$1" pin="E"/>
<wire x1="751.84" y1="182.88" x2="751.84" y2="210.82" width="0.1524" layer="91"/>
<wire x1="751.84" y1="210.82" x2="759.46" y2="210.82" width="0.1524" layer="91"/>
<junction x="751.84" y="182.88"/>
<pinref part="LED31" gate="G$1" pin="E"/>
<wire x1="751.84" y1="210.82" x2="751.84" y2="238.76" width="0.1524" layer="91"/>
<wire x1="751.84" y1="238.76" x2="759.46" y2="238.76" width="0.1524" layer="91"/>
<junction x="751.84" y="210.82"/>
</segment>
</net>
<net name="N$44" class="0">
<segment>
<pinref part="LED36" gate="G$1" pin="D"/>
<wire x1="759.46" y1="99.06" x2="749.3" y2="99.06" width="0.1524" layer="91"/>
<pinref part="LED35" gate="G$1" pin="D"/>
<wire x1="749.3" y1="99.06" x2="749.3" y2="129.54" width="0.1524" layer="91"/>
<wire x1="749.3" y1="129.54" x2="759.46" y2="129.54" width="0.1524" layer="91"/>
<pinref part="LED34" gate="G$1" pin="D"/>
<wire x1="749.3" y1="129.54" x2="749.3" y2="157.48" width="0.1524" layer="91"/>
<wire x1="749.3" y1="157.48" x2="759.46" y2="157.48" width="0.1524" layer="91"/>
<junction x="749.3" y="129.54"/>
<pinref part="LED33" gate="G$1" pin="D"/>
<wire x1="749.3" y1="157.48" x2="749.3" y2="185.42" width="0.1524" layer="91"/>
<wire x1="749.3" y1="185.42" x2="759.46" y2="185.42" width="0.1524" layer="91"/>
<junction x="749.3" y="157.48"/>
<pinref part="LED32" gate="G$1" pin="D"/>
<wire x1="749.3" y1="185.42" x2="749.3" y2="213.36" width="0.1524" layer="91"/>
<wire x1="749.3" y1="213.36" x2="759.46" y2="213.36" width="0.1524" layer="91"/>
<junction x="749.3" y="185.42"/>
<pinref part="LED31" gate="G$1" pin="D"/>
<wire x1="749.3" y1="213.36" x2="749.3" y2="241.3" width="0.1524" layer="91"/>
<wire x1="749.3" y1="241.3" x2="759.46" y2="241.3" width="0.1524" layer="91"/>
<junction x="749.3" y="213.36"/>
</segment>
</net>
<net name="N$45" class="0">
<segment>
<pinref part="LED36" gate="G$1" pin="C"/>
<wire x1="759.46" y1="101.6" x2="746.76" y2="101.6" width="0.1524" layer="91"/>
<pinref part="LED35" gate="G$1" pin="C"/>
<wire x1="746.76" y1="101.6" x2="746.76" y2="132.08" width="0.1524" layer="91"/>
<wire x1="746.76" y1="132.08" x2="759.46" y2="132.08" width="0.1524" layer="91"/>
<pinref part="LED34" gate="G$1" pin="C"/>
<wire x1="746.76" y1="132.08" x2="746.76" y2="160.02" width="0.1524" layer="91"/>
<wire x1="746.76" y1="160.02" x2="759.46" y2="160.02" width="0.1524" layer="91"/>
<junction x="746.76" y="132.08"/>
<pinref part="LED33" gate="G$1" pin="C"/>
<wire x1="746.76" y1="160.02" x2="746.76" y2="187.96" width="0.1524" layer="91"/>
<wire x1="746.76" y1="187.96" x2="759.46" y2="187.96" width="0.1524" layer="91"/>
<junction x="746.76" y="160.02"/>
<pinref part="LED32" gate="G$1" pin="C"/>
<wire x1="746.76" y1="187.96" x2="746.76" y2="215.9" width="0.1524" layer="91"/>
<wire x1="746.76" y1="215.9" x2="759.46" y2="215.9" width="0.1524" layer="91"/>
<junction x="746.76" y="187.96"/>
<pinref part="LED31" gate="G$1" pin="C"/>
<wire x1="746.76" y1="215.9" x2="746.76" y2="243.84" width="0.1524" layer="91"/>
<wire x1="746.76" y1="243.84" x2="759.46" y2="243.84" width="0.1524" layer="91"/>
<junction x="746.76" y="215.9"/>
</segment>
</net>
<net name="N$46" class="0">
<segment>
<pinref part="LED36" gate="G$1" pin="B"/>
<wire x1="759.46" y1="104.14" x2="744.22" y2="104.14" width="0.1524" layer="91"/>
<pinref part="LED35" gate="G$1" pin="B"/>
<wire x1="744.22" y1="104.14" x2="744.22" y2="134.62" width="0.1524" layer="91"/>
<wire x1="744.22" y1="134.62" x2="759.46" y2="134.62" width="0.1524" layer="91"/>
<pinref part="LED34" gate="G$1" pin="B"/>
<wire x1="744.22" y1="134.62" x2="744.22" y2="162.56" width="0.1524" layer="91"/>
<wire x1="744.22" y1="162.56" x2="759.46" y2="162.56" width="0.1524" layer="91"/>
<junction x="744.22" y="134.62"/>
<pinref part="LED33" gate="G$1" pin="B"/>
<wire x1="744.22" y1="162.56" x2="744.22" y2="190.5" width="0.1524" layer="91"/>
<wire x1="744.22" y1="190.5" x2="759.46" y2="190.5" width="0.1524" layer="91"/>
<junction x="744.22" y="162.56"/>
<pinref part="LED32" gate="G$1" pin="B"/>
<wire x1="744.22" y1="190.5" x2="744.22" y2="218.44" width="0.1524" layer="91"/>
<wire x1="744.22" y1="218.44" x2="759.46" y2="218.44" width="0.1524" layer="91"/>
<junction x="744.22" y="190.5"/>
<pinref part="LED31" gate="G$1" pin="B"/>
<wire x1="744.22" y1="218.44" x2="744.22" y2="246.38" width="0.1524" layer="91"/>
<wire x1="744.22" y1="246.38" x2="759.46" y2="246.38" width="0.1524" layer="91"/>
<junction x="744.22" y="218.44"/>
</segment>
</net>
<net name="N$47" class="0">
<segment>
<pinref part="LED36" gate="G$1" pin="A"/>
<wire x1="759.46" y1="106.68" x2="741.68" y2="106.68" width="0.1524" layer="91"/>
<pinref part="LED35" gate="G$1" pin="A"/>
<wire x1="741.68" y1="106.68" x2="741.68" y2="137.16" width="0.1524" layer="91"/>
<wire x1="741.68" y1="137.16" x2="759.46" y2="137.16" width="0.1524" layer="91"/>
<pinref part="LED34" gate="G$1" pin="A"/>
<wire x1="741.68" y1="137.16" x2="741.68" y2="165.1" width="0.1524" layer="91"/>
<wire x1="741.68" y1="165.1" x2="759.46" y2="165.1" width="0.1524" layer="91"/>
<junction x="741.68" y="137.16"/>
<pinref part="LED33" gate="G$1" pin="A"/>
<wire x1="741.68" y1="165.1" x2="741.68" y2="193.04" width="0.1524" layer="91"/>
<wire x1="741.68" y1="193.04" x2="759.46" y2="193.04" width="0.1524" layer="91"/>
<junction x="741.68" y="165.1"/>
<pinref part="LED32" gate="G$1" pin="A"/>
<wire x1="741.68" y1="193.04" x2="741.68" y2="220.98" width="0.1524" layer="91"/>
<wire x1="741.68" y1="220.98" x2="759.46" y2="220.98" width="0.1524" layer="91"/>
<junction x="741.68" y="193.04"/>
<pinref part="LED31" gate="G$1" pin="A"/>
<wire x1="741.68" y1="220.98" x2="741.68" y2="248.92" width="0.1524" layer="91"/>
<wire x1="741.68" y1="248.92" x2="759.46" y2="248.92" width="0.1524" layer="91"/>
<junction x="741.68" y="220.98"/>
</segment>
</net>
<net name="N$48" class="0">
<segment>
<pinref part="LED31" gate="G$1" pin="DP"/>
<wire x1="779.78" y1="233.68" x2="784.86" y2="233.68" width="0.1524" layer="91"/>
<wire x1="784.86" y1="233.68" x2="784.86" y2="205.74" width="0.1524" layer="91"/>
<pinref part="LED32" gate="G$1" pin="DP"/>
<wire x1="784.86" y1="205.74" x2="779.78" y2="205.74" width="0.1524" layer="91"/>
<pinref part="LED33" gate="G$1" pin="DP"/>
<wire x1="784.86" y1="205.74" x2="784.86" y2="177.8" width="0.1524" layer="91"/>
<wire x1="784.86" y1="177.8" x2="779.78" y2="177.8" width="0.1524" layer="91"/>
<junction x="784.86" y="205.74"/>
<pinref part="LED34" gate="G$1" pin="DP"/>
<wire x1="784.86" y1="177.8" x2="784.86" y2="149.86" width="0.1524" layer="91"/>
<wire x1="784.86" y1="149.86" x2="779.78" y2="149.86" width="0.1524" layer="91"/>
<junction x="784.86" y="177.8"/>
<pinref part="LED35" gate="G$1" pin="DP"/>
<wire x1="784.86" y1="149.86" x2="784.86" y2="121.92" width="0.1524" layer="91"/>
<wire x1="784.86" y1="121.92" x2="779.78" y2="121.92" width="0.1524" layer="91"/>
<junction x="784.86" y="149.86"/>
<pinref part="LED36" gate="G$1" pin="DP"/>
<wire x1="784.86" y1="121.92" x2="784.86" y2="91.44" width="0.1524" layer="91"/>
<wire x1="784.86" y1="91.44" x2="779.78" y2="91.44" width="0.1524" layer="91"/>
<junction x="784.86" y="121.92"/>
</segment>
</net>
<net name="N$81" class="0">
<segment>
<pinref part="LED61" gate="G$1" pin="A"/>
<wire x1="853.44" y1="248.92" x2="835.66" y2="248.92" width="0.1524" layer="91"/>
<wire x1="835.66" y1="248.92" x2="835.66" y2="220.98" width="0.1524" layer="91"/>
<pinref part="LED62" gate="G$1" pin="A"/>
<wire x1="835.66" y1="220.98" x2="853.44" y2="220.98" width="0.1524" layer="91"/>
<wire x1="835.66" y1="220.98" x2="835.66" y2="193.04" width="0.1524" layer="91"/>
<junction x="835.66" y="220.98"/>
<pinref part="LED63" gate="G$1" pin="A"/>
<wire x1="835.66" y1="193.04" x2="853.44" y2="193.04" width="0.1524" layer="91"/>
<wire x1="835.66" y1="193.04" x2="835.66" y2="165.1" width="0.1524" layer="91"/>
<junction x="835.66" y="193.04"/>
<pinref part="LED64" gate="G$1" pin="A"/>
<wire x1="835.66" y1="165.1" x2="853.44" y2="165.1" width="0.1524" layer="91"/>
<wire x1="835.66" y1="165.1" x2="835.66" y2="137.16" width="0.1524" layer="91"/>
<junction x="835.66" y="165.1"/>
<pinref part="LED65" gate="G$1" pin="A"/>
<wire x1="835.66" y1="137.16" x2="853.44" y2="137.16" width="0.1524" layer="91"/>
<wire x1="835.66" y1="137.16" x2="835.66" y2="106.68" width="0.1524" layer="91"/>
<junction x="835.66" y="137.16"/>
<pinref part="LED66" gate="G$1" pin="A"/>
<wire x1="835.66" y1="106.68" x2="853.44" y2="106.68" width="0.1524" layer="91"/>
</segment>
</net>
<net name="N$82" class="0">
<segment>
<pinref part="LED61" gate="G$1" pin="B"/>
<wire x1="853.44" y1="246.38" x2="838.2" y2="246.38" width="0.1524" layer="91"/>
<wire x1="838.2" y1="246.38" x2="838.2" y2="218.44" width="0.1524" layer="91"/>
<pinref part="LED62" gate="G$1" pin="B"/>
<wire x1="838.2" y1="218.44" x2="853.44" y2="218.44" width="0.1524" layer="91"/>
<wire x1="838.2" y1="218.44" x2="838.2" y2="190.5" width="0.1524" layer="91"/>
<junction x="838.2" y="218.44"/>
<pinref part="LED63" gate="G$1" pin="B"/>
<wire x1="838.2" y1="190.5" x2="853.44" y2="190.5" width="0.1524" layer="91"/>
<wire x1="838.2" y1="190.5" x2="838.2" y2="162.56" width="0.1524" layer="91"/>
<junction x="838.2" y="190.5"/>
<pinref part="LED64" gate="G$1" pin="B"/>
<wire x1="838.2" y1="162.56" x2="853.44" y2="162.56" width="0.1524" layer="91"/>
<wire x1="838.2" y1="162.56" x2="838.2" y2="134.62" width="0.1524" layer="91"/>
<junction x="838.2" y="162.56"/>
<pinref part="LED66" gate="G$1" pin="B"/>
<wire x1="838.2" y1="134.62" x2="838.2" y2="132.08" width="0.1524" layer="91"/>
<wire x1="838.2" y1="132.08" x2="838.2" y2="104.14" width="0.1524" layer="91"/>
<wire x1="838.2" y1="104.14" x2="853.44" y2="104.14" width="0.1524" layer="91"/>
<pinref part="LED65" gate="G$1" pin="B"/>
<wire x1="853.44" y1="134.62" x2="838.2" y2="134.62" width="0.1524" layer="91"/>
<junction x="838.2" y="134.62"/>
</segment>
</net>
<net name="N$83" class="0">
<segment>
<pinref part="LED66" gate="G$1" pin="G"/>
<wire x1="853.44" y1="91.44" x2="850.9" y2="91.44" width="0.1524" layer="91"/>
<wire x1="850.9" y1="91.44" x2="850.9" y2="121.92" width="0.1524" layer="91"/>
<pinref part="LED65" gate="G$1" pin="G"/>
<wire x1="850.9" y1="121.92" x2="853.44" y2="121.92" width="0.1524" layer="91"/>
<wire x1="850.9" y1="121.92" x2="850.9" y2="149.86" width="0.1524" layer="91"/>
<junction x="850.9" y="121.92"/>
<pinref part="LED64" gate="G$1" pin="G"/>
<wire x1="850.9" y1="149.86" x2="853.44" y2="149.86" width="0.1524" layer="91"/>
<wire x1="850.9" y1="149.86" x2="850.9" y2="177.8" width="0.1524" layer="91"/>
<junction x="850.9" y="149.86"/>
<pinref part="LED63" gate="G$1" pin="G"/>
<wire x1="850.9" y1="177.8" x2="853.44" y2="177.8" width="0.1524" layer="91"/>
<pinref part="LED62" gate="G$1" pin="G"/>
<wire x1="850.9" y1="177.8" x2="850.9" y2="205.74" width="0.1524" layer="91"/>
<wire x1="850.9" y1="205.74" x2="853.44" y2="205.74" width="0.1524" layer="91"/>
<junction x="850.9" y="177.8"/>
<pinref part="LED61" gate="G$1" pin="G"/>
<wire x1="850.9" y1="205.74" x2="850.9" y2="233.68" width="0.1524" layer="91"/>
<wire x1="850.9" y1="233.68" x2="853.44" y2="233.68" width="0.1524" layer="91"/>
<junction x="850.9" y="205.74"/>
</segment>
</net>
<net name="N$84" class="0">
<segment>
<pinref part="LED66" gate="G$1" pin="F"/>
<wire x1="853.44" y1="93.98" x2="848.36" y2="93.98" width="0.1524" layer="91"/>
<wire x1="848.36" y1="93.98" x2="848.36" y2="124.46" width="0.1524" layer="91"/>
<pinref part="LED65" gate="G$1" pin="F"/>
<wire x1="848.36" y1="124.46" x2="853.44" y2="124.46" width="0.1524" layer="91"/>
<wire x1="848.36" y1="124.46" x2="848.36" y2="152.4" width="0.1524" layer="91"/>
<junction x="848.36" y="124.46"/>
<pinref part="LED64" gate="G$1" pin="F"/>
<wire x1="848.36" y1="152.4" x2="853.44" y2="152.4" width="0.1524" layer="91"/>
<wire x1="848.36" y1="152.4" x2="848.36" y2="180.34" width="0.1524" layer="91"/>
<junction x="848.36" y="152.4"/>
<pinref part="LED63" gate="G$1" pin="F"/>
<wire x1="848.36" y1="180.34" x2="853.44" y2="180.34" width="0.1524" layer="91"/>
<wire x1="848.36" y1="180.34" x2="848.36" y2="208.28" width="0.1524" layer="91"/>
<junction x="848.36" y="180.34"/>
<pinref part="LED62" gate="G$1" pin="F"/>
<wire x1="848.36" y1="208.28" x2="853.44" y2="208.28" width="0.1524" layer="91"/>
<wire x1="848.36" y1="208.28" x2="848.36" y2="236.22" width="0.1524" layer="91"/>
<junction x="848.36" y="208.28"/>
<pinref part="LED61" gate="G$1" pin="F"/>
<wire x1="848.36" y1="236.22" x2="853.44" y2="236.22" width="0.1524" layer="91"/>
</segment>
</net>
<net name="N$85" class="0">
<segment>
<pinref part="LED66" gate="G$1" pin="E"/>
<wire x1="853.44" y1="96.52" x2="845.82" y2="96.52" width="0.1524" layer="91"/>
<wire x1="845.82" y1="96.52" x2="845.82" y2="127" width="0.1524" layer="91"/>
<pinref part="LED65" gate="G$1" pin="E"/>
<wire x1="845.82" y1="127" x2="853.44" y2="127" width="0.1524" layer="91"/>
<wire x1="845.82" y1="127" x2="845.82" y2="154.94" width="0.1524" layer="91"/>
<junction x="845.82" y="127"/>
<pinref part="LED64" gate="G$1" pin="E"/>
<wire x1="845.82" y1="154.94" x2="853.44" y2="154.94" width="0.1524" layer="91"/>
<wire x1="845.82" y1="154.94" x2="845.82" y2="182.88" width="0.1524" layer="91"/>
<junction x="845.82" y="154.94"/>
<pinref part="LED63" gate="G$1" pin="E"/>
<wire x1="845.82" y1="182.88" x2="853.44" y2="182.88" width="0.1524" layer="91"/>
<pinref part="LED62" gate="G$1" pin="E"/>
<wire x1="853.44" y1="210.82" x2="845.82" y2="210.82" width="0.1524" layer="91"/>
<wire x1="845.82" y1="210.82" x2="845.82" y2="182.88" width="0.1524" layer="91"/>
<junction x="845.82" y="182.88"/>
<pinref part="LED61" gate="G$1" pin="E"/>
<wire x1="853.44" y1="238.76" x2="845.82" y2="238.76" width="0.1524" layer="91"/>
<wire x1="845.82" y1="238.76" x2="845.82" y2="210.82" width="0.1524" layer="91"/>
<junction x="845.82" y="210.82"/>
</segment>
</net>
<net name="N$86" class="0">
<segment>
<pinref part="LED61" gate="G$1" pin="D"/>
<wire x1="853.44" y1="241.3" x2="843.28" y2="241.3" width="0.1524" layer="91"/>
<wire x1="843.28" y1="241.3" x2="843.28" y2="213.36" width="0.1524" layer="91"/>
<pinref part="LED62" gate="G$1" pin="D"/>
<wire x1="843.28" y1="213.36" x2="853.44" y2="213.36" width="0.1524" layer="91"/>
<pinref part="LED63" gate="G$1" pin="D"/>
<wire x1="843.28" y1="213.36" x2="843.28" y2="185.42" width="0.1524" layer="91"/>
<wire x1="843.28" y1="185.42" x2="853.44" y2="185.42" width="0.1524" layer="91"/>
<junction x="843.28" y="213.36"/>
<pinref part="LED64" gate="G$1" pin="D"/>
<wire x1="843.28" y1="185.42" x2="843.28" y2="157.48" width="0.1524" layer="91"/>
<wire x1="843.28" y1="157.48" x2="853.44" y2="157.48" width="0.1524" layer="91"/>
<junction x="843.28" y="185.42"/>
<pinref part="LED65" gate="G$1" pin="D"/>
<wire x1="843.28" y1="157.48" x2="843.28" y2="129.54" width="0.1524" layer="91"/>
<wire x1="843.28" y1="129.54" x2="853.44" y2="129.54" width="0.1524" layer="91"/>
<junction x="843.28" y="157.48"/>
<pinref part="LED66" gate="G$1" pin="D"/>
<wire x1="843.28" y1="129.54" x2="843.28" y2="99.06" width="0.1524" layer="91"/>
<wire x1="843.28" y1="99.06" x2="853.44" y2="99.06" width="0.1524" layer="91"/>
<junction x="843.28" y="129.54"/>
</segment>
</net>
<net name="N$87" class="0">
<segment>
<pinref part="LED61" gate="G$1" pin="C"/>
<wire x1="853.44" y1="243.84" x2="840.74" y2="243.84" width="0.1524" layer="91"/>
<pinref part="LED62" gate="G$1" pin="C"/>
<wire x1="840.74" y1="243.84" x2="840.74" y2="215.9" width="0.1524" layer="91"/>
<wire x1="840.74" y1="215.9" x2="853.44" y2="215.9" width="0.1524" layer="91"/>
<pinref part="LED63" gate="G$1" pin="C"/>
<wire x1="840.74" y1="215.9" x2="840.74" y2="187.96" width="0.1524" layer="91"/>
<wire x1="840.74" y1="187.96" x2="853.44" y2="187.96" width="0.1524" layer="91"/>
<junction x="840.74" y="215.9"/>
<pinref part="LED64" gate="G$1" pin="C"/>
<wire x1="840.74" y1="187.96" x2="840.74" y2="160.02" width="0.1524" layer="91"/>
<wire x1="840.74" y1="160.02" x2="853.44" y2="160.02" width="0.1524" layer="91"/>
<junction x="840.74" y="187.96"/>
<pinref part="LED65" gate="G$1" pin="C"/>
<wire x1="840.74" y1="160.02" x2="840.74" y2="132.08" width="0.1524" layer="91"/>
<wire x1="840.74" y1="132.08" x2="853.44" y2="132.08" width="0.1524" layer="91"/>
<junction x="840.74" y="160.02"/>
<pinref part="LED66" gate="G$1" pin="C"/>
<wire x1="840.74" y1="132.08" x2="840.74" y2="101.6" width="0.1524" layer="91"/>
<wire x1="840.74" y1="101.6" x2="853.44" y2="101.6" width="0.1524" layer="91"/>
<junction x="840.74" y="132.08"/>
</segment>
</net>
<net name="N$88" class="0">
<segment>
<pinref part="LED61" gate="G$1" pin="DP"/>
<wire x1="873.76" y1="233.68" x2="878.84" y2="233.68" width="0.1524" layer="91"/>
<wire x1="878.84" y1="233.68" x2="878.84" y2="205.74" width="0.1524" layer="91"/>
<pinref part="LED62" gate="G$1" pin="DP"/>
<wire x1="878.84" y1="205.74" x2="873.76" y2="205.74" width="0.1524" layer="91"/>
<pinref part="LED63" gate="G$1" pin="DP"/>
<wire x1="878.84" y1="205.74" x2="878.84" y2="177.8" width="0.1524" layer="91"/>
<wire x1="878.84" y1="177.8" x2="873.76" y2="177.8" width="0.1524" layer="91"/>
<junction x="878.84" y="205.74"/>
<pinref part="LED64" gate="G$1" pin="DP"/>
<wire x1="878.84" y1="177.8" x2="878.84" y2="149.86" width="0.1524" layer="91"/>
<wire x1="878.84" y1="149.86" x2="873.76" y2="149.86" width="0.1524" layer="91"/>
<junction x="878.84" y="177.8"/>
<pinref part="LED65" gate="G$1" pin="DP"/>
<wire x1="878.84" y1="149.86" x2="878.84" y2="121.92" width="0.1524" layer="91"/>
<wire x1="878.84" y1="121.92" x2="873.76" y2="121.92" width="0.1524" layer="91"/>
<junction x="878.84" y="149.86"/>
<pinref part="LED66" gate="G$1" pin="DP"/>
<wire x1="878.84" y1="121.92" x2="878.84" y2="91.44" width="0.1524" layer="91"/>
<wire x1="878.84" y1="91.44" x2="873.76" y2="91.44" width="0.1524" layer="91"/>
<junction x="878.84" y="121.92"/>
</segment>
</net>
<net name="N$89" class="0">
<segment>
<pinref part="LED72" gate="G$1" pin="G"/>
<wire x1="911.86" y1="91.44" x2="909.32" y2="91.44" width="0.1524" layer="91"/>
<wire x1="909.32" y1="91.44" x2="909.32" y2="121.92" width="0.1524" layer="91"/>
<pinref part="LED71" gate="G$1" pin="G"/>
<wire x1="911.86" y1="121.92" x2="909.32" y2="121.92" width="0.1524" layer="91"/>
<pinref part="LED70" gate="G$1" pin="G"/>
<wire x1="909.32" y1="121.92" x2="909.32" y2="149.86" width="0.1524" layer="91"/>
<wire x1="909.32" y1="149.86" x2="911.86" y2="149.86" width="0.1524" layer="91"/>
<junction x="909.32" y="121.92"/>
<wire x1="909.32" y1="149.86" x2="909.32" y2="177.8" width="0.1524" layer="91"/>
<junction x="909.32" y="149.86"/>
<pinref part="LED69" gate="G$1" pin="G"/>
<wire x1="909.32" y1="177.8" x2="911.86" y2="177.8" width="0.1524" layer="91"/>
<pinref part="LED68" gate="G$1" pin="G"/>
<wire x1="909.32" y1="177.8" x2="909.32" y2="205.74" width="0.1524" layer="91"/>
<wire x1="909.32" y1="205.74" x2="911.86" y2="205.74" width="0.1524" layer="91"/>
<junction x="909.32" y="177.8"/>
<pinref part="LED67" gate="G$1" pin="G"/>
<wire x1="909.32" y1="205.74" x2="909.32" y2="233.68" width="0.1524" layer="91"/>
<wire x1="909.32" y1="233.68" x2="911.86" y2="233.68" width="0.1524" layer="91"/>
<junction x="909.32" y="205.74"/>
</segment>
</net>
<net name="N$90" class="0">
<segment>
<pinref part="LED72" gate="G$1" pin="F"/>
<wire x1="911.86" y1="93.98" x2="906.78" y2="93.98" width="0.1524" layer="91"/>
<pinref part="LED71" gate="G$1" pin="F"/>
<wire x1="906.78" y1="93.98" x2="906.78" y2="124.46" width="0.1524" layer="91"/>
<wire x1="906.78" y1="124.46" x2="911.86" y2="124.46" width="0.1524" layer="91"/>
<pinref part="LED70" gate="G$1" pin="F"/>
<wire x1="906.78" y1="124.46" x2="906.78" y2="152.4" width="0.1524" layer="91"/>
<wire x1="906.78" y1="152.4" x2="911.86" y2="152.4" width="0.1524" layer="91"/>
<junction x="906.78" y="124.46"/>
<pinref part="LED69" gate="G$1" pin="F"/>
<wire x1="906.78" y1="152.4" x2="906.78" y2="180.34" width="0.1524" layer="91"/>
<wire x1="906.78" y1="180.34" x2="911.86" y2="180.34" width="0.1524" layer="91"/>
<junction x="906.78" y="152.4"/>
<pinref part="LED68" gate="G$1" pin="F"/>
<wire x1="906.78" y1="180.34" x2="906.78" y2="208.28" width="0.1524" layer="91"/>
<wire x1="906.78" y1="208.28" x2="911.86" y2="208.28" width="0.1524" layer="91"/>
<junction x="906.78" y="180.34"/>
<pinref part="LED67" gate="G$1" pin="F"/>
<wire x1="906.78" y1="208.28" x2="906.78" y2="236.22" width="0.1524" layer="91"/>
<wire x1="906.78" y1="236.22" x2="911.86" y2="236.22" width="0.1524" layer="91"/>
<junction x="906.78" y="208.28"/>
</segment>
</net>
<net name="N$91" class="0">
<segment>
<pinref part="LED72" gate="G$1" pin="E"/>
<wire x1="911.86" y1="96.52" x2="904.24" y2="96.52" width="0.1524" layer="91"/>
<pinref part="LED71" gate="G$1" pin="E"/>
<wire x1="904.24" y1="96.52" x2="904.24" y2="127" width="0.1524" layer="91"/>
<wire x1="904.24" y1="127" x2="911.86" y2="127" width="0.1524" layer="91"/>
<pinref part="LED70" gate="G$1" pin="E"/>
<wire x1="904.24" y1="127" x2="904.24" y2="154.94" width="0.1524" layer="91"/>
<wire x1="904.24" y1="154.94" x2="911.86" y2="154.94" width="0.1524" layer="91"/>
<junction x="904.24" y="127"/>
<pinref part="LED69" gate="G$1" pin="E"/>
<wire x1="904.24" y1="154.94" x2="904.24" y2="182.88" width="0.1524" layer="91"/>
<wire x1="904.24" y1="182.88" x2="911.86" y2="182.88" width="0.1524" layer="91"/>
<junction x="904.24" y="154.94"/>
<pinref part="LED68" gate="G$1" pin="E"/>
<wire x1="904.24" y1="182.88" x2="904.24" y2="210.82" width="0.1524" layer="91"/>
<wire x1="904.24" y1="210.82" x2="911.86" y2="210.82" width="0.1524" layer="91"/>
<junction x="904.24" y="182.88"/>
<pinref part="LED67" gate="G$1" pin="E"/>
<wire x1="904.24" y1="210.82" x2="904.24" y2="238.76" width="0.1524" layer="91"/>
<wire x1="904.24" y1="238.76" x2="911.86" y2="238.76" width="0.1524" layer="91"/>
<junction x="904.24" y="210.82"/>
</segment>
</net>
<net name="N$92" class="0">
<segment>
<pinref part="LED72" gate="G$1" pin="D"/>
<wire x1="911.86" y1="99.06" x2="901.7" y2="99.06" width="0.1524" layer="91"/>
<pinref part="LED71" gate="G$1" pin="D"/>
<wire x1="901.7" y1="99.06" x2="901.7" y2="129.54" width="0.1524" layer="91"/>
<wire x1="901.7" y1="129.54" x2="911.86" y2="129.54" width="0.1524" layer="91"/>
<pinref part="LED70" gate="G$1" pin="D"/>
<wire x1="901.7" y1="129.54" x2="901.7" y2="157.48" width="0.1524" layer="91"/>
<wire x1="901.7" y1="157.48" x2="911.86" y2="157.48" width="0.1524" layer="91"/>
<junction x="901.7" y="129.54"/>
<pinref part="LED69" gate="G$1" pin="D"/>
<wire x1="901.7" y1="157.48" x2="901.7" y2="185.42" width="0.1524" layer="91"/>
<wire x1="901.7" y1="185.42" x2="911.86" y2="185.42" width="0.1524" layer="91"/>
<junction x="901.7" y="157.48"/>
<pinref part="LED68" gate="G$1" pin="D"/>
<wire x1="901.7" y1="185.42" x2="901.7" y2="213.36" width="0.1524" layer="91"/>
<wire x1="901.7" y1="213.36" x2="911.86" y2="213.36" width="0.1524" layer="91"/>
<junction x="901.7" y="185.42"/>
<pinref part="LED67" gate="G$1" pin="D"/>
<wire x1="901.7" y1="213.36" x2="901.7" y2="241.3" width="0.1524" layer="91"/>
<wire x1="901.7" y1="241.3" x2="911.86" y2="241.3" width="0.1524" layer="91"/>
<junction x="901.7" y="213.36"/>
</segment>
</net>
<net name="N$93" class="0">
<segment>
<pinref part="LED72" gate="G$1" pin="C"/>
<wire x1="911.86" y1="101.6" x2="899.16" y2="101.6" width="0.1524" layer="91"/>
<pinref part="LED71" gate="G$1" pin="C"/>
<wire x1="899.16" y1="101.6" x2="899.16" y2="132.08" width="0.1524" layer="91"/>
<wire x1="899.16" y1="132.08" x2="911.86" y2="132.08" width="0.1524" layer="91"/>
<pinref part="LED70" gate="G$1" pin="C"/>
<wire x1="899.16" y1="132.08" x2="899.16" y2="160.02" width="0.1524" layer="91"/>
<wire x1="899.16" y1="160.02" x2="911.86" y2="160.02" width="0.1524" layer="91"/>
<junction x="899.16" y="132.08"/>
<pinref part="LED69" gate="G$1" pin="C"/>
<wire x1="899.16" y1="160.02" x2="899.16" y2="187.96" width="0.1524" layer="91"/>
<wire x1="899.16" y1="187.96" x2="911.86" y2="187.96" width="0.1524" layer="91"/>
<junction x="899.16" y="160.02"/>
<pinref part="LED68" gate="G$1" pin="C"/>
<wire x1="899.16" y1="187.96" x2="899.16" y2="215.9" width="0.1524" layer="91"/>
<wire x1="899.16" y1="215.9" x2="911.86" y2="215.9" width="0.1524" layer="91"/>
<junction x="899.16" y="187.96"/>
<pinref part="LED67" gate="G$1" pin="C"/>
<wire x1="899.16" y1="215.9" x2="899.16" y2="243.84" width="0.1524" layer="91"/>
<wire x1="899.16" y1="243.84" x2="911.86" y2="243.84" width="0.1524" layer="91"/>
<junction x="899.16" y="215.9"/>
</segment>
</net>
<net name="N$94" class="0">
<segment>
<pinref part="LED72" gate="G$1" pin="B"/>
<wire x1="911.86" y1="104.14" x2="896.62" y2="104.14" width="0.1524" layer="91"/>
<pinref part="LED71" gate="G$1" pin="B"/>
<wire x1="896.62" y1="104.14" x2="896.62" y2="134.62" width="0.1524" layer="91"/>
<wire x1="896.62" y1="134.62" x2="911.86" y2="134.62" width="0.1524" layer="91"/>
<pinref part="LED70" gate="G$1" pin="B"/>
<wire x1="896.62" y1="134.62" x2="896.62" y2="162.56" width="0.1524" layer="91"/>
<wire x1="896.62" y1="162.56" x2="911.86" y2="162.56" width="0.1524" layer="91"/>
<junction x="896.62" y="134.62"/>
<pinref part="LED69" gate="G$1" pin="B"/>
<wire x1="896.62" y1="162.56" x2="896.62" y2="190.5" width="0.1524" layer="91"/>
<wire x1="896.62" y1="190.5" x2="911.86" y2="190.5" width="0.1524" layer="91"/>
<junction x="896.62" y="162.56"/>
<pinref part="LED68" gate="G$1" pin="B"/>
<wire x1="896.62" y1="190.5" x2="896.62" y2="218.44" width="0.1524" layer="91"/>
<wire x1="896.62" y1="218.44" x2="911.86" y2="218.44" width="0.1524" layer="91"/>
<junction x="896.62" y="190.5"/>
<pinref part="LED67" gate="G$1" pin="B"/>
<wire x1="896.62" y1="218.44" x2="896.62" y2="246.38" width="0.1524" layer="91"/>
<wire x1="896.62" y1="246.38" x2="911.86" y2="246.38" width="0.1524" layer="91"/>
<junction x="896.62" y="218.44"/>
</segment>
</net>
<net name="N$95" class="0">
<segment>
<pinref part="LED72" gate="G$1" pin="A"/>
<wire x1="911.86" y1="106.68" x2="894.08" y2="106.68" width="0.1524" layer="91"/>
<pinref part="LED71" gate="G$1" pin="A"/>
<wire x1="894.08" y1="106.68" x2="894.08" y2="137.16" width="0.1524" layer="91"/>
<wire x1="894.08" y1="137.16" x2="911.86" y2="137.16" width="0.1524" layer="91"/>
<pinref part="LED70" gate="G$1" pin="A"/>
<wire x1="894.08" y1="137.16" x2="894.08" y2="165.1" width="0.1524" layer="91"/>
<wire x1="894.08" y1="165.1" x2="911.86" y2="165.1" width="0.1524" layer="91"/>
<junction x="894.08" y="137.16"/>
<pinref part="LED69" gate="G$1" pin="A"/>
<wire x1="894.08" y1="165.1" x2="894.08" y2="193.04" width="0.1524" layer="91"/>
<wire x1="894.08" y1="193.04" x2="911.86" y2="193.04" width="0.1524" layer="91"/>
<junction x="894.08" y="165.1"/>
<pinref part="LED68" gate="G$1" pin="A"/>
<wire x1="894.08" y1="193.04" x2="894.08" y2="220.98" width="0.1524" layer="91"/>
<wire x1="894.08" y1="220.98" x2="911.86" y2="220.98" width="0.1524" layer="91"/>
<junction x="894.08" y="193.04"/>
<pinref part="LED67" gate="G$1" pin="A"/>
<wire x1="894.08" y1="220.98" x2="894.08" y2="248.92" width="0.1524" layer="91"/>
<wire x1="894.08" y1="248.92" x2="911.86" y2="248.92" width="0.1524" layer="91"/>
<junction x="894.08" y="220.98"/>
</segment>
</net>
<net name="N$96" class="0">
<segment>
<pinref part="LED67" gate="G$1" pin="DP"/>
<wire x1="932.18" y1="233.68" x2="937.26" y2="233.68" width="0.1524" layer="91"/>
<wire x1="937.26" y1="233.68" x2="937.26" y2="205.74" width="0.1524" layer="91"/>
<pinref part="LED68" gate="G$1" pin="DP"/>
<wire x1="937.26" y1="205.74" x2="932.18" y2="205.74" width="0.1524" layer="91"/>
<pinref part="LED69" gate="G$1" pin="DP"/>
<wire x1="937.26" y1="205.74" x2="937.26" y2="177.8" width="0.1524" layer="91"/>
<wire x1="937.26" y1="177.8" x2="932.18" y2="177.8" width="0.1524" layer="91"/>
<junction x="937.26" y="205.74"/>
<pinref part="LED70" gate="G$1" pin="DP"/>
<wire x1="937.26" y1="177.8" x2="937.26" y2="149.86" width="0.1524" layer="91"/>
<wire x1="937.26" y1="149.86" x2="932.18" y2="149.86" width="0.1524" layer="91"/>
<junction x="937.26" y="177.8"/>
<pinref part="LED71" gate="G$1" pin="DP"/>
<wire x1="937.26" y1="149.86" x2="937.26" y2="121.92" width="0.1524" layer="91"/>
<wire x1="937.26" y1="121.92" x2="932.18" y2="121.92" width="0.1524" layer="91"/>
<junction x="937.26" y="149.86"/>
<pinref part="LED72" gate="G$1" pin="DP"/>
<wire x1="937.26" y1="121.92" x2="937.26" y2="91.44" width="0.1524" layer="91"/>
<wire x1="937.26" y1="91.44" x2="932.18" y2="91.44" width="0.1524" layer="91"/>
<junction x="937.26" y="121.92"/>
</segment>
</net>
</nets>
</sheet>
</sheets>
</schematic>
</drawing>
<compatibility>
<note version="8.2" severity="warning">
Since Version 8.2, EAGLE supports online libraries. The ids
of those online libraries will not be understood (or retained)
with this version.
</note>
<note version="8.3" severity="warning">
Since Version 8.3, EAGLE supports URNs for individual library
assets (packages, symbols, and devices). The URNs of those assets
will not be understood (or retained) with this version.
</note>
<note version="8.3" severity="warning">
Since Version 8.3, EAGLE supports the association of 3D packages
with devices in libraries, schematics, and board files. Those 3D
packages will not be understood (or retained) with this version.
</note>
</compatibility>
</eagle>
