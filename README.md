# pblua

pblua is a C module for lua to encode/decode protobuf messages.

# Install
```sh
git clone https://github.com/cosiner/pblua
cd pblua
make
sudo make install
```

# Usage
```lua
require('pblua')

--- .pb file is generated by command 'protoc -o protobuf.pb PROTOFILE...'
local codecU = pblua.loadfile('/path/to/protobuf.pb')
--- or 
local codecA =  pblua.loadstring('content of .pb file')

local userEncoded = codecU:encode('pkg.User', {
    Name = 'Foo',
    Age = 1
})

local user = codecU:decode('pkg.User', userEncoded)

print(user.Name, user.Age)

local articleEncoded = codecA:encode('pkg.Article', {
    Title= '......',
    Author = '',
    Date = ''
})

local article = codecA:decode('pkg.Article', articleEncoded)

print(article.Title, article.Author)
```

# License
MIT.