import json
import urllib.request

# Download the JSON data
url = "https://chromiumdash.appspot.com/fetch_releases?channel=Stable&platform=&num=1&offset=0"
response = urllib.request.urlopen(url)
data = response.read().decode("utf-8")

# Parse the JSON data
json_data = json.loads(data)

# Extract the version numbers
for release in json_data:
    platform = release.get("platform", "Unknown")
    version = release.get("version", "Unknown")
    print(f"Platform: {platform}, Version: {version}")
