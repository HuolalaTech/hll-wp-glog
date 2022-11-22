Pod::Spec.new do |s|

  s.name         = "GlogCore"
  s.version      = "0.0.1"
  s.summary      = "GlogCore"
  s.author       = { '货拉拉' => '货拉拉' }

  s.description  = <<-DESC
  Glog
                   DESC

  s.homepage     = "https://xxx.com"
  s.license      = { :type => "Apache", :file => "LICENSE.TXT"}

  s.ios.deployment_target = "9.0"

  s.source       = { :git => "", :tag => "#{s.version}" }
  s.source_files =  "Core/**/*.{h,m,mm,cpp}"
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

