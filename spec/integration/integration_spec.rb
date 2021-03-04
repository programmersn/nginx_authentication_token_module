require "spec_helper"

describe "Integration specs >> " do

    describe "Bootstrap >> " do
        it "Nginx webserver is up" do
            http = Curl.get("http://localhost:8888") 

            expect(http.response_code).to be_between(200, 299).inclusive.or be_between(300, 399).inclusive
        end

        it "Backend app is up" do
            http = Curl.get("http://localhost:4567")

            expect(http.response_code).to be_between(200, 299).inclusive    
        end

        it "Redis server is up" do
            response = Redis.new.ping

            expect(response).to eq("PONG")
        end
    end

    describe "Module use cases >> " do
        before(:all) do 
            @redis = Redis.new
            @redis.set("testcookie", "nouaim")
        end
        
        it "Redirected : Incoming request doesn't contain 'auth-token' cookie header" do 
            http = Curl.get("http://localhost:8888") do |http|
                http.headers["Cookie"] = ""
            end
            
            expect(http.response_code).to be_between(300, 399).inclusive
        end

        it "Redirect: No user matching 'auth-token' cookie's value found in database" do 
            http = Curl.get("http://localhost:8888") do |http|
                http.headers["Cookie"] = "auth-token=invalid"     
            end
            
            expect(http.response_code).to be_between(300, 399).inclusive
        end

        it "Sent back user ID to client: found user matching 'auth-token' cookie's value in database" do
            http = Curl.get("http://localhost:8888") do |http|
                http.headers["Cookie"] = "auth-token=testcookie"
            end
        
            expect(http.response_code).to be_between(200, 299).inclusive
        end
    end
end