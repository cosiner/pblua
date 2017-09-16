package test

import (
	"encoding/json"
	"io/ioutil"
	"testing"
	"time"
)

func TestPBDecode(t *testing.T) {
	content, err := ioutil.ReadFile("../build/testout/pb.encode")
	if err != nil {
		t.Fatal(err)
	}
	now := time.Now()
	for i := 0; i < 200000; i++ {
		u := new(User)
		err = u.Unmarshal(content)
		if err != nil {
			t.Fatal(err)
		}
		if i == 0 {
			c, _ := json.Marshal(u)
			t.Log(string(c))
		}
	}
	t.Log(time.Now().Sub(now))
}
