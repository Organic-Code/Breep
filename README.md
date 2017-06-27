# Breep

## What is Breep?

Breep is a c++ bridged peer to peer library. What does that mean? It means that even though
the network is constructed as a peer to peer one, there may be no direct connections between
two dudes, say A and B, if these connections are impossible (due to poorly configured ports, for example).
In that case, Breep will attempt to use another peer C as a bridge between A and B.
<br/>
Breep is also a high-level library. You don't have to care on when peers connect, disconnect,
send data, and on how to send your classes. You simply register listeners that get notified when
peers come and go, when they send you stuff. You may even use serialization and send your own object
directly through the network. Same goes for your listeners: you don't say 'I want to listen for
piles of bytes', but instead you say 'I want to listen for fancy::MyClass'.


### How do I use Breep::network ?

The best way to now it is to read the [tutorials](tutorials/). Alternatively, you may read some [examples](examples/) and [the online doc](https://organic-code.github.io/Breep/). But as a little preview, here is a 'Hello World!'-type example:

Here is how to create a network, start listening on port 1234, and send "Hello!" to any budy that connects:
```cpp
BREEP_DECLARE_TYPE(std::string)

void co_listener(breep::tcp::network& network, const breep::tcp::peer& source) {
	network.send_object_to(source, std::string("Hello!"));
}

int main() {
	breep::tcp::network network(1234);
	network.add_connection_listener(&co_listener);
	network.sync_awake();
	return 0;
}
```
The ``BREEP_DECLARE_TYPE`` involved here is used to tell to breep::network that we will listen/send some std::string*s*.
If you forget to do it, you will get a compile-time error.

There is how to do the opposite: the network starts listening on port 1233, tries to connect at localhost:1234, prints the first message it sees, then disconnect:
```cpp
BREEP_DECLARE_TYPE(std::string)

void data_listener(breep::tcp::netdata_wrapper<std::string>& dw) {
    std::cout << dw.data << std::endl;
    dw.network.disconnect();
}

int main() {
    breep::tcp::network network(1233);
    network.add_data_listener<std::string>(&data_listener);
    if (!network.connect(boost::asio::ip::address_v4::loopback(), 1234)) {
        std::cout << "Failed to connect.\n";
        return 1;
    }
    network.join();
    return 0;
}
```
Please don't get confused: there is no UDP in this lib (yet).


### Why should I use Breep::network ?

* It's awesome!
* It's high level: you can directly send and receive objects.
* The overhead for this is low: if you set up well your serialization, you only have a fixed 64bits extra overhead (compared to sending raw bytes to the p2p network — in comparison, TCP has 320bits of overhead only for its header)
* It's easy to get in: just read the examples, you'll see!

### Why should I NOT use Breep::network ?

* It has not been tested as much as it should have been.
* It's probably broken for BigEndian architecture (I have no way to test this, sorry ; a warning should be displayed on such architectures.)
* It's very, *very* slow to compile with.

## Requirements

| Resource                       | Requirement               |
|:------------------------------:|:-------------------------:|
| Compiler                       | C++14 compliant or above  |
| [Boost](http://www.boost.org/) | Boost 1.55 or above       |



## Road Map

| Milestone                                                | Feature                             | Status      |
|:--------------------------------------------------------:|:-----------------------------------:|:-----------:|
| 0.1                                                      | Peer to peer network management     | complete    |
| 0.1                                                      | Instantiated objects delivery       | complete    |
| [1.0](https://github.com/Organic-Code/Breep/milestone/1) | Improved serialization              | complete    |
| [1.0](https://github.com/Organic-Code/Breep/milestone/1) | Multiple objects delivery in one go | complete    |
| [x.x](https://github.com/Organic-Code/Breep/issues/1)    | Client server network management    | on hold     |

The project is currently on testing stage before the release of Breep 1.0.0

## License

This work is under the [European Union Public License v1.1](LICENSE.md).

You may get a copy of this license in your language from the European Commission [here](https://joinup.ec.europa.eu/community/eupl/og_page/european-union-public-licence-eupl-v11).

Extract of article 13 :

    All linguistic versions of this Licence, approved by the European Commission, have identical value.
    Parties can take advantage of the linguistic version of their choice.

## Author

[Lucas Lazare](https://github.com/Organic-code), an IT student frustrated from not being able to properly use java's broken network library, and inspired by [KryoNet](https://github.com/EsotericSoftware/kryonet)

