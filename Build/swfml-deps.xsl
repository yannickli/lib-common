<?xml version="1.0"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version='1.0'>
    <xsl:output method="text" indent="no" />
    <xsl:param name="target" />
    <xsl:param name="reldir" />
    <xsl:strip-space elements="*" />

    <xsl:template match="clip[@import]">
        <xsl:value-of select="$reldir" /><xsl:value-of select="$target" /><xsl:text>: </xsl:text>
        <xsl:value-of select="$reldir" /><xsl:value-of select="@import" /><xsl:text>&#10;</xsl:text>
        <xsl:value-of select="$reldir" /><xsl:value-of select="@import" /><xsl:text>:&#10;</xsl:text>
        <xsl:text>&#10;</xsl:text>
    </xsl:template>
</xsl:stylesheet>
