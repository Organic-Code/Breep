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
piles of bytes', but instead you say 'I want to listen for this class'.


## License

This work is under the [European Union Public License v1.1](LICENSE.md).

You may get a copy of this license in your language from the European Commission [here](https://joinup.ec.europa.eu/community/eupl/og_page/european-union-public-licence-eupl-v11).

Extract of article 13 :

    All linguistic versions of this Licence, approved by the European Commission, have identical value.
    Parties can take advantage of the linguistic version of their choice.
