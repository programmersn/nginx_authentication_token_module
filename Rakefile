require 'rake'
require 'rspec/core/rake_task'

RSpec::Core::RakeTask.new(:integration) do |t|
  t.pattern = 'spec/**/*_spec.rb'
end

namespace :nginx do
  desc "Start nginx"
  task :start do
    `./build/nginx/sbin/nginx`
  end

  desc "Stop nginx"
  task :stop do  
    `./build/nginx/sbin/nginx -s stop || pkill nginx`
  end

  desc "Restart nginx"
  task :restart => [:stop, :start]

  desc "Recompile nginx"
  task :compile do 
    sh "./scripts/nginx-compile-module.sh"
  end
end

namespace :redis do
  desc "Start redis"
  task :start do
    `$(which redis-server)`
  end

  desc "Stop redis"
  task :stop do  
    `$(which redis-cli) shutdown`
  end

  desc "Restart redis server"
  task :restart => [:stop, :start]
end

desc "Bootstraps the local development environment"
task :bootstrap do 
  unless Dir.exists?("build") and Dir.exists?("vendor") 
    sh "./scripts/nginx-bootstrap.sh"
  end
end

desc "Run integration tests"
task :default => [:bootstrap, "redis:start", "nginx:start", :integration, "nginx:stop", "redis:stop"]
