<?xml version="1.0" encoding="windows-1252"?>
<xsl:stylesheet xmlns:mv="http://matrix-vision.de/xml" version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:template match="/">
    <html>
      <head>
        <title>
          <xsl:value-of select="mvIMPACT_acquireLogFile/@name"/>
        </title>
        <style type="text/css">
          body         { font: 8pt/11pt courier new; }
          td           { font: 8pt/11pt courier new; vertical-align: text-top;}
          h1           { font: 13pt/15pt courier new; }
          .csux_title  { font: 16pt courier new; font-weight: bold; }
          .error       { font: bold; font-size: 10pt; color: #FF0000; }
          .tabletitle  { background-color: #CCCCCC; color: #000000; font-weight: bold; }
          .df0         { background-color: #EEEEEE; color: #000000; }
          .df1         { background-color: #E0E0E0; color: #000000; }
          .df2         { background-color: #FFE000; color: #EE0000; }
          .df3         { background-color: #FF0000; color: #000000; }
          .df4         { background-color: #FF00DD; color: #000000; }
          .df5         { background-color: #E0E0E0; color: #000000; }
          .df6         { background-color: #E0E0E0; color: #000000; }
          .df31        { background-color: #0000FF; color: #FFFFFF; }
          .dfb0        { background-color: #EEEEEE; color: #000000; font-weight: bold; }
          .dfb1        { background-color: #E0E0E0; color: #000000; font-weight: bold; }
          .dfb2        { background-color: #FFE000; color: #EE0000; font-weight: bold; }
          .dfb3        { background-color: #FF0000; color: #000000; font-weight: bold; }
          .dfb4        { background-color: #FF00DD; color: #000000; font-weight: bold; }
          .dfb5        { background-color: #E0E0E0; color: #000000; font-weight: bold; }
          .dfb6        { background-color: #E0E0E0; color: #000000; font-weight: bold; }
          .dfb31       { background-color: #0000FF; color: #FFFFFF; font-weight: bold; }
        </style>
      </head>
      <body link="#FF0000" vlink="#FF0000" alink="#800000">
        <xsl:apply-templates select="mvIMPACT_acquireLogFile"/>
      </body>
    </html>
  </xsl:template>
  <xsl:template match="mvIMPACT_acquireLogFile">
    <!-- page header -->
    <table border="0" width="100%">
      <tr>
        <td width="100%" class="csux_title">
          <xsl:value-of select="./@name"/>
        </td>
        <td rowspan="2">
          <img align="right" alt="MATRIX VISION GmbH">
            <xsl:attribute name="src">logo.gif</xsl:attribute>
          </img>
        </td>
      </tr>
      <tr>
      </tr>
    </table>
    <!-- table for the log messages -->
    <table border="0" width="100%">
      <tr>
        <xsl:if test="logmsg/@ts">
          <td width="20" class="tabletitle">&#160;Timestamp&#160;</td>
        </xsl:if>
        <xsl:if test="logmsg/@td">
          <td width="20" class="tabletitle">
            &#160;Time&#160;<br></br>&#160;diff.&#160;
          </td>
        </xsl:if>
        <xsl:if test="logmsg/@procId">
          <td width="20" class="tabletitle">&#160;proc.&#160;Id&#160;</td>
        </xsl:if>
        <xsl:if test="logmsg/@threadId">
          <td width="20" class="tabletitle">&#160;thread&#160;Id&#160;</td>
        </xsl:if>
        <xsl:if test="logmsg/@df">
          <td width="20" class="tabletitle">
            &#160;Msg.&#160;<br></br>&#160;type&#160;
          </td>
        </xsl:if>
        <xsl:if test="logmsg/@orgFile">
          <td width="20" class="tabletitle">&#160;orginal&#160;file&#160;</td>
        </xsl:if>
        <xsl:if test="logmsg/@msg">
          <td width="500" class="tabletitle">&#160;Message&#160;</td>
        </xsl:if>
      </tr>
      <xsl:apply-templates select="logmsg"></xsl:apply-templates>
    </table>
  </xsl:template>
  <!-- template for a log message -->
  <xsl:template match="logmsg">
    <tr>
      <!-- the time stamp -->
      <xsl:choose>
        <xsl:when test="./@ts">
          <td align="right">
            <xsl:attribute name="class">
              df<xsl:value-of select="@df"/>
            </xsl:attribute>
            <xsl:value-of select="@ts"/>&#160;
          </td>
        </xsl:when>
        <xsl:otherwise>
          <td> </td>
        </xsl:otherwise>
      </xsl:choose>

      <!-- The time difference -->
      <xsl:choose>
        <xsl:when test="./@td">
          <td align="right">
            <xsl:attribute name="class">
              df<xsl:value-of select="@df"/>
            </xsl:attribute>
            <xsl:value-of select="@td"/>&#160;
          </td>
        </xsl:when>
        <xsl:otherwise>
          <td> </td>
        </xsl:otherwise>
      </xsl:choose>

      <!-- The process ID -->
      <xsl:choose>
        <xsl:when test="./@procId">
          <td align="left">
            <xsl:attribute name="class">
              df<xsl:value-of select="@df"/>
            </xsl:attribute>
            <xsl:value-of select="@procId"/>&#160;
          </td>
        </xsl:when>
      </xsl:choose>

      <!-- The thread ID -->
      <xsl:choose>
        <xsl:when test="./@threadId">
          <td align="left">
            <xsl:attribute name="class">
              df<xsl:value-of select="@df"/>
            </xsl:attribute>
            <xsl:value-of select="@threadId"/>&#160;
          </td>
        </xsl:when>
      </xsl:choose>

      <!-- The type of message -->
      <xsl:choose>
        <xsl:when test="./@df">
          <td align="center">
            <xsl:attribute name="class">
              df<xsl:value-of select="@df"/>
            </xsl:attribute>
            <xsl:value-of select="@df"/>&#160;
          </td>
        </xsl:when>
        <xsl:otherwise>
          <td> </td>
        </xsl:otherwise>
      </xsl:choose>

      <!-- The original file name -->
      <xsl:choose>
        <xsl:when test="./@orgFile">
          <td align="left">
            <xsl:attribute name="class">
              df<xsl:value-of select="@df"/>
            </xsl:attribute>
            <xsl:value-of select="@orgFile"/>&#160;
          </td>
        </xsl:when>
      </xsl:choose>

      <!-- The message text -->
      <xsl:choose>
        <xsl:when test="./@msg">
          <td align="left">
            <xsl:attribute name="class">
              df<xsl:value-of select="@df"/>
            </xsl:attribute>
            <xsl:value-of select="@msg"/>&#160;
          </td>
        </xsl:when>
        <xsl:otherwise>
          <td> </td>
        </xsl:otherwise>
      </xsl:choose>
    </tr>
  </xsl:template>
</xsl:stylesheet>
