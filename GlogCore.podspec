Pod::Spec.new do |s|

  s.name         = "GlogCore"
  s.version      = "1.0.1"
  s.summary      = "Glog is a cross platform high-performance log storage framework developed by Huolala."
  s.author       = { 'issac.zeng' => 'extremetsa@gmail.com' }

  s.description  = <<-DESC
                      The Glog for Objective-C.
                      Glog is a format free, cross platform, high performance log storage framework.
                   DESC

  s.homepage     = "https://github.com/HuolalaTech/hll-wp-glog"
  s.license      = { :type => "Apache 2.0", :file => "LICENSE.TXT"}

  s.ios.deployment_target = "9.0"

  s.source       = { :git => "https://github.com/HuolalaTech/hll-wp-glog.git", :tag => "v#{s.version}" }
  s.source_files =  "Core/**/*.{h,m,mm,cpp,hpp}", "Core/micro-ecc/*.{h,cpp,inc}", "Core/openssl/*.{h,S,cpp}"
  s.public_header_files = "Core/**/*.h"
  s.framework    = "CoreFoundation"
  s.libraries    = "z", "c++"
  s.compiler_flags = '-x objective-c++'
  s.pod_target_xcconfig = {
    "CLANG_CXX_LANGUAGE_STANDARD" => "gnu++17",
    "CLANG_CXX_LIBRARY" => "libc++",
    "CLANG_WARN_OBJC_IMPLICIT_RETAIN_SELF" => "NO",
  }
end

