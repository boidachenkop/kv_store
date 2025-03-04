# dummy tester

import requests
from dataclasses import dataclass
import random
import string
import time

url = "http://localhost:1270"


@dataclass
class ResponseStatus:
    status_code: int
    body: str

    def __str__(self):
        return f"Response: status {self.status_code}, body: \n {self.body}"


def push_item(key: str, value: str):
    payload = f"{key}\n{value}"
    headers = {"Content-Type": "text/plain"}
    response = requests.post(f"{url}/item", data=payload, headers=headers)
    return ResponseStatus(response.status_code, response.text)


def get_item(key: str):
    response = requests.get(f"{url}/item?item={key}")
    return ResponseStatus(response.status_code, response.text)


def delete_item(key: str):
    response = requests.delete(f"{url}/item?item={key}")
    return ResponseStatus(response.status_code, response.text)


def get_all():
    response = requests.get(f"{url}/all")
    return ResponseStatus(response.status_code, response.text)


def test1():
    push_item("key", "value")
    resp = get_item("key")
    print(f"{resp}")
    output = resp.body.split("\n")
    assert output[0] == "key"
    assert output[1] == "value"


def test2():
    key_vals = {}
    for _ in range(100):
        key_length = random.randint(1, 255)
        value_length = random.randint(1, 2000)

        key = "".join(
            random.choices(string.ascii_letters + string.digits, k=key_length)
        )
        value = "".join(
            random.choices(string.ascii_letters + string.digits, k=value_length)
        )

        key_vals[key] = value
    for k, v in key_vals.items():
        resp = push_item(k, v)
        print(f"{resp}")
        assert resp.status_code == 200
        resp = get_item(k)
        assert resp.status_code == 200
        print(f"{resp}")
        output = resp.body.split("\n")
        assert output[0] == k
        assert output[1] == v

    key_vals = dict(sorted(key_vals.items()))
    resp = get_all()
    assert resp.status_code == 200
    resp_key_vals = resp.body.split('\n')
    print(f"EXPECT {len(resp_key_vals)} == {len(key_vals)}")
    assert len(resp_key_vals) == len(key_vals) + 1
    i = 0
    for k, v in key_vals.items():
        if not resp_key_vals[i]:
            break
        rk, rv = resp_key_vals[i].split(' : ')
        print(f"EXPECT {rk} == {k}")
        assert rk == k
        assert rv == v
        i += 1


if __name__ == "__main__":
    # test1()
    test2()
