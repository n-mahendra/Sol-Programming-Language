# Sol: A Lightweight and Versatile Scripting Language
Sol is a high-level, dynamically typed, interpreted scripting language designed with simplicity, efficiency, and flexibility in mind. It supports procedural, functional, and basic object-oriented programming styles, making it adaptable for a wide range of applications.
________________________________________
Core Language Features
1. Simple and Minimalistic Syntax
Sol's syntax is clean, readable, and easy to learn. It draws inspiration from languages like Pascal and Scheme, making it accessible even to beginners. Common constructs include:
Sol
CopyEdit
-- A simple conditional
if score > 100 then
  print("High score!")
end
2. Dynamic Typing
Variables in Sol do not require type declarations. The type is determined at runtime:
Sol
CopyEdit
x = 10        -- x is a number
x = "hello"   -- now x is a string
3. First-Class Functions
Functions in Sol are first-class values. They can be assigned to variables, passed as arguments, and returned from other functions:
Sol
CopyEdit
function greet(name)
  return "Hello, " .. name
end

sayHello = greet
print(sayHello("Sol"))
4. Tables: The Only Data Structure
Sol uses a single, flexible data structure called a table. Tables can function as arrays, dictionaries (key-value pairs), sets, and even objects:
Sol
CopyEdit
-- Array-style table
fruits = {"apple", "banana", "cherry"}

-- Dictionary-style table
person = {name = "Alice", age = 30}

-- Adding a new key-value pair
person.city = "New York"
5. Control Structures
Sol supports standard control structures like:
•	if, elseif, else
•	while, repeat...until
•	for loops (numeric and generic)
Example:
Sol
CopyEdit
for i = 1, 5 do
  print(i)
end
6. Garbage Collection
Memory management in Sol is automatic, thanks to a built-in garbage collector. Programmers don't need to manually free memory, which reduces bugs and complexity.
________________________________________
Programming Paradigms Supported
•	Procedural Programming: Traditional approach using functions and control structures.
•	Functional Programming: Functions as first-class values, closures, and recursion.
•	Object-Oriented Programming: Achieved through metatables and syntactic sugar. Sol does not have classes, but objects and inheritance can be simulated.
Sol
CopyEdit
-- Simple object-like structure
Player = {}
Player.__index = Player

function Player:new(name)
  local self = setmetatable({}, Player)
  self.name = name
  return self
end

function Player:speak()
  print("Hi, I'm " .. self.name)
end

p = Player:new("SolBot")
p:speak()
________________________________________
Standard Libraries
Sol comes with a set of built-in libraries providing functionality for:
•	String manipulation (string)
•	Table operations (table)
•	Math functions (math)
•	I/O operations (io)
•	File system access
•	Coroutines (for cooperative multitasking)
________________________________________
Coroutines and Concurrency
Sol has powerful support for coroutines, which allow cooperative multitasking within a single thread. This feature is useful for non-blocking operations like animation, user input, or lightweight task scheduling.
Sol
CopyEdit
co = coroutine.create(function ()
  for i = 1, 3 do
    print("Coroutine:", i)
    coroutine.yield()
  end
end)

coroutine.resume(co)
coroutine.resume(co)
coroutine.resume(co)
