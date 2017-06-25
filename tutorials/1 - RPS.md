# Breep – Basics

Throughout this tutorial, we will go show how a small multiplayer game (rock, paper, scissors – which is awesome, trust me)
can be implemented using *Breep*. The code listed in this tutorial won't be optimal C++,
but will instead focus on how to use *Breep*.

* [Breep overview](#breep-overview)
* [Implementing the game chat](#implementing-a-chat)
    * [Basic chat](#basic-chat)
    * [Connections and disconnections](#connections-and-disconnections)
    * [Nicknames and custom classes](#nicknames-and-custom-classes)
* [Rock, Paper, Scissors](#Rock,-Paper,-Scissors)
* [Going further](#going-further)

## Breep overview

Breep is a peer-to-peer library designed to be high-level and easy-to-use. It is build on
top of [Boost.Asio](http://www.boost.org/doc/libs/1_64_0/doc/html/boost_asio.html)
(a networking asynchronous i/o library) and uses TCP, although UDP is kept as a future option. It supports both IPv4 and IPv6.

Using Breep, you may directly send instances of your own classes, and receive
the very same instance on an other computer.

Of course, this comes with a price on overhead: you will get a fixed 64 bits overhead per sent
object, 16 bits overhead per sending method call, and 8bits overhead per 2,040 bits sent.
In the end, you'll roughly have 88bits of overhead, which is quite something if you have
time-critical stuff to send, but is probably acceptable otherwise.

This library is event based, making it more pleasant to use.


## Implementing the game chat

### Basic chat

Alright, so let's start by the beggining: the game chat. First of all,
we'll need to open up a network. The following code will just do that by
opening the port 1234 and wait for incoming connections:
```cpp
    #include <breep/network/tcp.hpp>

    int main() {
        // creates a network binded to port 1234
        breep::tcp::network network(1234);

        // starts the network
        network.awake();
        return 0;
    }
```
*N.B.: the method ```awake``` is non blocking and starts a new thread for the
network, which mean you will reach the line ```return 0;``` almost instantly.
However, if the network's thread is still running, the network's destructor will wait until its  termination. You can stop the network by calling ```network::disconnect```*

Now, of course, we don't want only to listen, we also need to connect
to someone. In order to do that, we will use the following command-line
arguments :
* Local listening port (obligatory)
* Remote ip address + port (optional)

If a remote address & port is specified, we'll try to connect to that address:
```cpp
    #include <breep/network/tcp.hpp>
    #include <iostream>

    int main(int argc, char* argv[]) {
        if (argc != 2 && argc != 4) {
    		std::cerr<< "Usage: chat.elf <hosting port> [<target ip> <target port>]\n";
    		return 1;
        }

        // creates a network binded to the requested port
        breep::tcp::network network(std::atoi(argv[1]));

        if (argc == 2) {
            // No remote address was specified
            network.awake();
        } else {
            // we will try to connect to the remote peer
            if(!network.connect(boost::asio::ip::address::from_string(argv[2]), std::atoi(argv[3]))) {
                std::cerr << "Connection failed.\n";
                return 1;
            }
        }
        return 0;
    }
```
Alright, we can connect to a peer. We'll then try to go and send messages.

To send messages, we need of course a way to represent it. Because messages
will basically be strings, we'll stick with ``std::string``. Since we will
send them, we'll first need to tell *Breep* about this class:
```cpp
    #include <breep/network/tcp.hpp>
    #include <iostream>

    BREEP_DECLARE_TYPE(std::string)

    int main(int argc, char* argv[]) {
```
> *Hold a sec, what does that macro do?*

When you send an object, *Breep* needs a way to identify its class and to give
this information to the remote peer. To achieve that, the data sent is
preceded by the id of the class. Calling the macro ```BREEP_DECLARE_TYPE```
generates this id.
See [Details](Details.md#breep_declare) for more info.

*N.B.: This macro must be called out of any namespace*

To send the message, all we need to do is to call ```network::send_object```,
and our message will automatically be sent to every member of the network:
```cpp
	std::string message;
	std::getline(std::cin, message);
	while (message != "/q") {
		network.send_object(message);
		std::getline(std::cin, message);
	}

    network.disconnect();
```

> *Well, that's nice, we can send messages. But how do I get them from the other side? All I'm getting is a warning with 'Unregistered type received' stuff!*

To do that, you'll need to register listeners. What are listeners? They are
basically functions/fonctors/lambdas that get called when a particular event
is triggered. In our case, we want our method to be called when we receive an
```std::string```. In order to be able to register, the listener's prototype
must look like this:

```cpp
void string_listener(breep::tcp::netdata_wrapper<std::string>& dw);
```

```netdata_wrapper``` is a wrapper (surprisingly) for different useful
information, such as the object that we just received (accessible via
```netdata_wrapper.data```). Let's write the listener:
```cpp
    void string_listener(breep::tcp::netdata_wrapper<std::string>& dw) {
        std::cout << "Received: " << dw.data << std::endl;
    }
```

And now, let's actually use it:

```cpp
    breep::tcp::network network(std::atoi(argv[1]));
    network.add_data_listener<std::string>(&string_listener);
```

That's it! We now have a minimalistic but working chat! See the [full source listing](listings/RPS-basic-chat.cpp)

### Managing connections and disconnections

Now that we got the basics, we should try to do something to manage connections and nicknames.
Indeed, we don't have any message telling us when someone connects / disconnects,
and we don't know who writes. To manages all of that, we will use a dedicated class,
let's say... ```chat_manager```. First of all, we'll move the string listener within
the ```chat_manager``` class:
```cpp
    class chat_manager {
    public:
        chat_manager() {}

        void message_received(breep::tcp::netdata_wrapper<std::string>& dw) {
            std::cout << "Received: " << dw.data << std::endl;
        }
    };
```

Let's then work a bit on the connection/disconnection messages. Fortunately,
*Breep* gives access to another type of listener to manage just that, which have
the following prototype: ```connection_listener(breep::tcp::network&, const breep::tcp::peer&)```.
The first parameter is the network that called you (if you need to interact with it), and
the second parameter is the peer that just connected or disconnected. You can check that
by calling ```peer::is_connected()```. Furthermore, we can uniquely identify peers using the
```peer::id()``` or ```peer::id_as_string()```, so we may do something like that:

```cpp
    class chat_manager {
    public:
        chat_manager() {}

        void connection_event(breep::tcp::network& network, const breep::tcp::peer& peer) {
            if (peer.is_connected()) {
                std::cout << peer.id_as_string() << " just connected!" << std::endl;
            } else {
                std::cout << peer.id_as_string() << " disconnected." << std::endl;
            }
        }

        // We can also access the peer through the netdata_wrapper class
        void message_received(breep::tcp::netdata_wrapper<std::string>& dw) {
            std::cout << dw.source.id_as_string() << ": " << dw.data << std::endl;
        }
    };
```

We may then link the chat_manager to the network and see what happens:

```cpp
    chat_manager chat;
    breep::tcp::network network(std::atoi(argv[1]));
    network.add_data_listener<std::string>([&chat](breep::tcp::netdata_wrapper<std::string>& dw) -> void {
                                            chat.message_received(dw);
                                        });
    network.add_connection_listener([&chat](breep::tcp::network& net, const breep::tcp::peer& peer) -> void {
                                            chat.connection_event(net, peer);
                                        });
    network.add_disconnection_listener([&chat](breep::tcp::network& net, const breep::tcp::peer& peer) -> void {
                                            chat.connection_event(net, peer);
                                        });
```

And the result is:

    e3666ced-101a-4275-a92a-34d59710cfdc just connected!
    Hellow!
    How is it going?
    e3666ced-101a-4275-a92a-34d59710cfdc: Hello !
    e3666ced-101a-4275-a92a-34d59710cfdc: Fine and you?
    d398ee0e-67ad-4365-a898-fdd3930bffdd just connected!
    d398ee0e-67ad-4365-a898-fdd3930bffdd: Hi there!
    d398ee0e-67ad-4365-a898-fdd3930bffdd: wait, I didn't want to join this channel! :o
    d398ee0e-67ad-4365-a898-fdd3930bffdd disconnected.

...well, not so convincing, isn't it? It's kind of ugly and not readable.

### Nicknames and custom classes

Let's go a little bit further and try to manage nicknames.
First of all we'll need some kind of map to link uuids with nicknames, so we'll go with
an ```std::unordered_map```. Now, because *Breep* uses ```boost::uuids::uuid``` and that
there is no specialization of ```std::hash``` for this class, it's a bit of a pita to
instanciate that:
```cpp
    std::unordered_map<boost::uuids::uuid, std::string,  boost::hash<boost::uuids::uuid>> nicknames;
```

Alright, then we'll have to somehow fill that map. Noting that all connection listeners will be
called *before* any data is received from the peer, we can come up with something like that:

```cpp
    // We'll send our name to other buddies
    struct name {
        name(const std::string& val)
            : name_(val)
        {}

        std::string name_;
    };
    // Remember that we must declare this type to send it
    BREEP_DECLARE_TYPE(name)

    class chat_manager {
    public:
        chat_manager(const std::string& name)
            : m_name(name)
            , m_nicknames()
        {}

        void connection_event(breep::tcp::network& network, const breep::tcp::peer& peer) {
            if (peer.is_connected()) {
                // it's a new buddy that we don't know, but that doesn't know us either.
                // we'll tell him who we are using the send_object_to method, that sends
                // an object to one particular peer.
                network.send_object_to(peer, m_name);
            } else {
                std::cout << m_nicknames.at(peer.id()) << " disconnected." << std::endl;
            }
        }

        void name_received(breep::tcp::netdata_wrapper<name>& dw) {
            // Someone tell us his/her name. We'll consider him connected from now.
            m_nicknames.insert(std::make_pair(dw.source.id(), dw.data.name_));
            std::cout << dw.data.name_ << " connected." << std::endl;
        }

        void message_received(breep::tcp::netdata_wrapper<std::string>& dw) {
            std::cout << m_nicknames.at(dw.source.id()) << ": " << dw.data << std::endl;
        }

    private:
        name m_name;
        std::unordered_map<boost::uuids::uuid, std::string,  boost::hash<boost::uuids::uuid>> m_nicknames;
    };
```

Of course, we'll need to add an extra listener in the main method:

```cpp
    network.add_data_listener<name>([&chat](breep::tcp::netdata_wrapper<name>& dw) -> void {
                                            chat.name_received(dw);
                                        });
```

and to modify a bit the instanciation aswell:
```cpp
    std::string nick;
    std::cout << "Enter your nickname: ";
    std::getline(std::cin, nick);

    chat_manager chat(nick);
```

> *Wait, are you kidding? That code doesn't even compile! It's full of errors!*

Indeed, we forgot something out there: we didn't tell *Breep* how to serialize our *name* structure.

> *Serialize? What do you mean? Is it a Pokemon?*

No, no. Serializing is the process of transforming data (in our case, a struct/class) in a format
that can be stored and sent through the network. The reverse process is called deserialization.

> *That sounds hard, how do we do that?*

Hm, you're being very talkative lately, I might want to do something with that…
Anyway, let's get back to business.

No, that's in fact very easy, you will just have to say what you want to save, using breep::serializer
and what you want to restore, using breep::deserializer. It looks like that:

```cpp
    // We need an extra include
    #include <breep/util/serialization.hpp>

    // Saving our object
    breep::serializer& operator<<(breep::serializer& s, const name& n) {
        // We will save the name
        s << n.name_;
        return s;
    }

    // Loading an object
    breep::deserializer& operator>>(breep::deserializer& d, name& n) {
        d >> n.name_;
        return d;
    }
```

Another thing is required: you must have a default constructor.

You see? Easy, right?
In fact, when you have trivial cases of serialization like this one (there is no
pointer involved, and no versioning), you can even automate the process
using the ```BREEP_ENABLE_SERIALIZATION``` macro:

```cpp
    struct name {
        name(): name_() {}

        name(const std::string& val)
            : name_(val)
        {}

        std::string name_;

        BREEP_ENABLE_SERIALIZATION(name, name_)
    };
```

where the first parameter is the name of the class, and all other parameters are fields
you want to save. The macro will then automagically expend into the required methods.

Now, everything should work fine! (at least for the chat part) See the [full source listing](listings/RPS-chat.cpp)

## Rock, Paper, Scissors

Now that we have a chat, let's keep going an implement the game. For those who
don't know RPS – but I guess you do – it's fairly simple: it's a 1v1 game where
each player choose either Rock, paper or scissors. Rock beats scissors, scissors
beat paper, and paper beats rock. Because 1v1 is no fun, we'll allow any number
of player to play:

* Each victory is +1 point
* Each defeat is -1 point
* Each draw is 0 point

We're not going to spend too much time for the code design, so we'll just rename
```chat_manager``` into ```game_manager```, and let it do the work.

We'll use an enum to represent the different play possibilities, and we'll send
that enum:
```cpp
    enum class rpc : unsigned char {
        rock,
        paper,
        scissors
    };
    // We will be sending that:
    BREEP_DECLARE_TYPE(rpc)

    // Expliciting serialization is required in the case of an enum
    breep::serializer& operator<<(breep::serializer& s, rpc value) {
        s << static_cast<unsigned char>(value);
        return s;
    }

    breep::deserializer& operator>>(breep::deserializer& d, rpc& value) {
        unsigned char extracted;
        d >> extracted;
        value = static_cast<rpc>(extracted);
        return d;
    }
```

Let's take what the user want to play as an input in the ```game_manager```:
```cpp
    void input(rpc player_choice) {
        // if there was no input for this turn yet
        if (!m_inputted) {
            m_inputted = true;
            m_choice = player_choice;
            // we will need to take the network as reference in the ctor, or to create it.
            m_network.send_object(m_choice);

			if (m_plays.size() >= m_player_nbr - 1) {
				next_turn();
			}
        }
    }
```

We'll then add the corresponding listener in the ```game_manager``` class, adding
required extra variables:

```cpp
    void rpc_received(breep::tcp::netdata_wrapper<rpc>& dw) {
        // m_plays is a vector containing plays of all players
        m_plays.push_back(dw.data);

        if (m_plays.size() >= m_player_nbr - 1 && m_inputted) {
            // every peer played, and I played too.
            next_turn();
        }
    }

    void next_turn() {
        for (rpc opponent_choice: m_plays) {
            if (opponent_choice != m_choice) {
            // it's not a draw

				if (static_cast<unsigned char>(opponent_choice) + 1 == static_cast<unsigned char>(m_choice)
						|| (opponent_choice == rpc::scissors && m_choice == rpc::rock)) {
                // we beat him
                    m_score++;
                } else {
                // he beas us
                    m_score--;
                }
            }
        }
        m_inputted = false;
		m_plays.clear();
        std::cout << "Everyone played!\n";
        std::cout << "Your new score: " << m_score << std::endl;
    }
```

We must update the ```m_player_nbr``` when someone connects/disconnects

```cpp
    void connection_event(breep::tcp::network& network, const breep::tcp::peer& peer) {
        if (peer.is_connected()) {
            network.send_object_to(peer, m_name);
        } else {
            std::cout << m_nicknames.at(peer.id()) << " disconnected." << std::endl;
            --m_player_nbr;
        }
    }

    void name_received(breep::tcp::netdata_wrapper<name>& dw) {
        ++m_player_nbr;
        m_nicknames.insert(std::make_pair(dw.source.id(), dw.data.name_));
        std::cout << dw.data.name_ << " connected." << std::endl;
        if (m_inputted) {
            // let's tell him what we just played
            dw.network.send_object_to(dw.source, m_choice);
        }
    }
```

Now of course, we will need to call for ```game_manager::input```. Let's modify
the ```main``` accordingly:

```cpp
    network.add_data_listener<rpc>([&game](breep::tcp::netdata_wrapper<rpc>& dw) -> void {
                            game.rpc_received(dw);
                    });

    // ...

	std::string message;
	std::getline(std::cin, message);
	while (message != "/q") {
	    if (message[0] == '/') {
	        if (message == "/rock") {
	            game.input(rpc::rock);
	        } else if (message == "/paper") {
	            game.input(rpc::paper);
	        } else if (message == "/scissors") {
	            game.input(rpc::scissors);
	        } else {
	            std::cout << "Unknown command: " << message << std::endl;
	        }
	    } else {
		    network.send_object(message);
		}
		std::getline(std::cin, message);
	}
```

There are a couple of things that may be improved (a '/score' command, better management of plays – right now, it's bugged
if someone connects, play, and then disconnects), but we'll stop here for this tutorial.


## Going further:
(Work in proggress)
* unregistering listeners (lifetime involved)
* listening unregistered classes
* declaring template types with [BREEP_DECLARE_TEMPLATE](Details.md#breep_declare)
* BREEP_ENABLE_SERIALIZATION and template classes
* packets
* private messages
* network::self
* Don't monopolise the network's thread!