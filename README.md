
# Sol: A Lightweight, High-level, Multi-paradigm Programming language  and Versatile Scripting Language. 
<br>
Sol is a high-level, dynamically typed, interpreted scripting language designed 
with simplicity, efficiency, and flexibility in mind. It supports procedural, 
functional, and basic object-oriented programming styles, making it adaptable 
for a wide range of applications.<br>
________________________________________<br>
Core Language Features<br>
1. Simple and Minimalistic Syntax<br>
Sol's syntax is clean, readable, and easy to learn. It draws inspiration from 
languages like Pascal and Scheme, making it accessible even to beginners. Common 
constructs include:<br>
<br>
Sol<br>
-- A simple conditional<br>
if score &gt; 100 then<br>
print(&quot;High score!&quot;)<br>
end<br>
<br>
2. Dynamic Typing<br>
Variables in Sol do not require type declarations. The type is determined at 
runtime:<br>
<br>
Sol<br>
x = 10 -- x is a number<br>
x = &quot;hello&quot; -- now x is a string<br>
<br>
3. First-Class Functions<br>
Functions in Sol are first-class values. They can be assigned to variables, 
passed as arguments, and returned from other functions:<br>
<br>
<br>
Sol<br>
function greet(name)<br>
return &quot;Hello, &quot; .. name<br>
end<br>
<br>
sayHello = greet<br>
print(sayHello(&quot;Sol&quot;))<br>
<br>
<br>
4. Tables: The Only Data Structure<br>
Sol uses a single, flexible data structure called a table. Tables can function 
as arrays, dictionaries (key-value pairs), sets, and even objects:<br>
<br>
Sol<br>
-- Array-style table<br>
fruits = {&quot;apple&quot;, &quot;banana&quot;, &quot;cherry&quot;}<br>
<br>
-- Dictionary-style table<br>
person = {name = &quot;Alice&quot;, age = 30}<br>
<br>
-- Adding a new key-value pair<br>
person.city = &quot;New York&quot;<br>
<br>
5. Control Structures<br>
Sol supports standard control structures like:<br>
• if, elseif, else<br>
• while, repeat...until<br>
• for loops (numeric and generic)<br>
<br>
Example:<br>
<br>
Sol<br>
for i = 1, 5 do<br>
print(i)<br>
end<br>
<br>
6. Garbage Collection<br>
Memory management in Sol is automatic, thanks to a built-in garbage collector. 
Programmers don't need to manually free memory, which reduces bugs and 
complexity.<br>
________________________________________<br>
Programming Paradigms Supported<br>
• Procedural Programming: Traditional approach using functions and control 
structures.<br>
• Functional Programming: Functions as first-class values, closures, and 
recursion.<br>
• Object-Oriented Programming: Achieved through metatables and syntactic sugar. 
Sol does not have classes, but objects and inheritance can be simulated.<br>
<br>
Sol<br>
-- Simple object-like structure<br>
Player = {}<br>
Player.__index = Player<br>
<br>
function Player:new(name)<br>
local self = setmetatable({}, Player)<br>
self.name = name<br>
return self<br>
end<br>
<br>
function Player:speak()<br>
print(&quot;Hi, I'm &quot; .. self.name)<br>
end<br>
<br>
p = Player:new(&quot;SolBot&quot;)<br>
p:speak()<br>
<br>
________________________________________<br>
Standard Libraries<br>
Sol comes with a set of built-in libraries providing functionality for:<br>
• String manipulation (string)<br>
• Table operations (table)<br>
• Math functions (math)<br>
• I/O operations (io)<br>
• File system access<br>
• Coroutines (for cooperative multitasking)<br>
<br>
________________________________________<br>
Coroutines and Concurrency<br>
Sol has powerful support for coroutines, which allow cooperative multitasking 
within a single thread. This feature is useful for non-blocking operations like 
animation, user input, or lightweight task scheduling.<br>
<br>
Sol<br>
<br>
co = coroutine.create(function ()<br>
for i = 1, 3 do<br>
print(&quot;Coroutine:&quot;, i)<br>
coroutine.yield()<br>
end<br>
end)<br>
<br>
coroutine.resume(co)<br>
coroutine.resume(co)<br>
coroutine.resume(co)</p>
