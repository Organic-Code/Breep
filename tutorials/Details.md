# Breep - Details

## BREEP_DECLARE
The BREEP_DECLARE_TYPE macro is a macro used to associate a portable and an hopefully unique hash to a class
(reinventing the RTTI for cross-platform interoperability).
It should be called with a fully-qualified class as parameter and outside of any namespace. If the name isn't fully qualified and the
'using' keyword is used, or if the name is an alias, the macro will still work but produce a different
hash for the class.

The macro expands to a structure specialization:
```cpp
    BREEP_DECLARE_TYPE(user::custom_class)

    // expands to

	namespace breep {
	    namespace detail {
	    	template <>
    		struct networking_traits_impl<user::custom_class> {
			    const std::string universal_name = std::string("user::custom_class");
		    };
	    }
	}
```

Which in turns allows the use of the structure type_traits:
```cpp
    namespace breep {
        template <>
        struct type_traits<user::custom_class> {

            // Returns the string "user::custom_class"
            static const std::string& universal_name();

            // Returns the hash of that string
            static uint64_t hash_code();
        };
    }
```


The ```hash()``` method is used to identify the class of the object at the other side of the network.
By the way, this specialization was already done for primitive types, so no need to call it for them.

When using template classes — say ```std::vector```, for example — it can be harsh to write
```cpp
    BREEP_DECLARE_TYPE(std::vector<int>)
    BREEP_DECLARE_TYPE(std::vector<double>)
    BREEP_DECLARE_TYPE(std::vector<std::string>)
    // ... ad vitam æternam
```

So, the macro ```BREEP_DECLARE_TEMPLATE``` is here to rescue you!
This second macro expends to a bit more complicated code (not going into the details), and allows you
simplify all that into:
```cpp
    BREEP_DECLARE_TEMPLATE(std::vector)
```

...well, almost, because in fact, ```std::vector<int>``` is really ```std::vector<int, std::allocator<int>>```,
so to obtain the expected result, you may use (the order is not important):
```cpp
    BREEP_DECLARE_TEMPLATE(std::allocator)
    BREEP_DECLARE_TEMPLATE(std::vector)
```

And yes, that mean you could technically use
```cpp
    BREEP_DECLARE_TEMPLATE(std::char_traits)
    BREEP_DECLARE_TEMPLATE(std::allocator)
    BREEP_DECLARE_TEMPLATE(std::basic_string)
```

over

```cpp
    BREEP_DECLARE_TYPE(std::string)
```

Note that this macro has some limitations: you can't use ```BREEP_DECLARE_TEMPLATE```
if any of the template parameters is a literal. You'll have to use ```BREEP_DECLARE_TYPE```
with the explicit specialization.
