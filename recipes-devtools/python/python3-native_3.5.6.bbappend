#Do not apply the following patch that will cause tensorflow-lite pip package
#build to fails
SRC_URI_remove += " file://unixccompiler.patch "
