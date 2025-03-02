# dummy tester

import requests

url = "http://localhost:1270"

if __name__ == "__main__":
  for i in range(100):
    try:
        response = requests.post(f"{url}/item")
        print(f"Status Code: {response.status_code}")
        print(f"Response Body:\n{response.text}")
    except requests.exceptions.RequestException as e:
        print(f"Request failed: {e}")