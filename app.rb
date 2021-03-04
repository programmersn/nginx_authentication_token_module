require 'sinatra'
require 'json'

get '/' do
    puts "Hello #{request.env['HTTP_USER_ID']}"
    "Hello #{request.env['HTTP_USER_ID']}"
    #puts JSON.pretty_generate(request.env)
end