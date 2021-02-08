require 'rake'
require 'rspec/core/rake_task'

RSpec::Core::RakeTask.new(:integration) do |t|
  t.pattern = 'spec/**/*_spec.rb'
end

namespace :nginx do
  desc "Starts nginx"
  task :start do
    `./build/nginx/sbin/nginx`
  end

  desc "Stops nginx"
  task :stop do  
    `./build/nginx/sbin/nginx -s stop`
  end

  desc "Recompiles nginx"
  task :compile do 
    sh "./scripts/nginx-compile.sh"
  end
end

desc "Bootstraps the local development environment"
task :bootstrap do 
  unless Dir.exists?("build") and Dir.exists?("vendor") 
    sh "./scripts/nginx-bootstrap.sh"
  end
end

desc "Run integration tests"
task :default => [:bootstrap, "nginx:start", :integration, "nginx:stop"]
