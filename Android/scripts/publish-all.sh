echo "publish all, glog_version: $1"
exec "./gradlew" "-Pglog_version=$1" "clean" ":glog:publishAll" "--no-build-cache"