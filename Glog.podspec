Pod::Spec.new do |s|

  s.name         = "Glog"
  s.version      = "0.0.1"
  s.summary      = "Glog"
  s.author       = { '货拉拉' => '货拉拉' }

  s.description  = <<-DESC
  Glog
                   DESC

  s.homepage     = "https://xxx.com"
  s.license      = { :type => "Apache", :file => "LICENSE.TXT"}

  s.ios.deployment_target = "9.0"

  s.source       = { :git => "", :tag => "#{s.version}" }
  s.source_files =  "iOS/Glog/Glog/*.{h,m,mm}"
  s.public_header_files = "iOS/Glog/Glog/*.h"

  s.framework    = "CoreFoundation"
  s.libraries    = "z", "c++"
  s.dependency "GlogCore"
end

