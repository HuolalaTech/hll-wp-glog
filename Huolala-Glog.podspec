Pod::Spec.new do |s|

  s.name         = "Huolala-Glog"
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
  s.source_files =  "iOS/Glog/Glog/*.{h,m,mm}"
  s.public_header_files = "iOS/Glog/Glog/*.h"

  s.framework    = "CoreFoundation"
  s.libraries    = "z", "c++"
  s.dependency 'GlogCore', '~> 1.0.1'
end

