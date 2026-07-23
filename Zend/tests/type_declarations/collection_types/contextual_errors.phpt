--TEST--
contextual collection literals: fail-closed diagnostics (compile-time and runtime)
--FILE--
<?php

/* A contextual literal with no usable expected type is rejected. Where the
 * source is known at compile time (return, non-collection context) the error is
 * a compile error; where the callee is known only at runtime (arguments) it is a
 * TypeError, naming the explicit fix in both cases. */

echo "== compile-time (each in its own process) ==\n";
$compile = [
    'no source (assignment)' => '$x = vec{1, 2};',
    'no source (echo)'       => 'echo vec{1};',
    'no source (array elem)' => '$a = [vec{1}];',
    'return not a collection'=> 'function f(): int { return vec{1}; }',
    'return wrong kind'      => 'function f(): vec[int] { return set{1}; }',
    'return no type'         => 'function f() { return vec{1}; }',
    'return tuple arity'     => 'function f(): tuple[int,string] { return tuple{1,2,3}; }',
    'not implemented (map)'  => 'function g(vec[int] $v){} g(map{1});',
    'const-expr position'    => 'const X = vec{1};',
    'named argument'         => 'function g(int $a, vec[int] $v){} g(v: vec{1}, a: 1);',
];
foreach ($compile as $label => $code) {
    $cmd = escapeshellarg(PHP_BINARY) . ' -n -r ' . escapeshellarg($code) . ' 2>&1';
    $out = shell_exec($cmd);
    if (preg_match('/(?:Fatal error|Parse error): (.+?) in /s', $out, $m)) {
        printf("%-24s %s\n", $label, trim($m[1]));
    } else {
        printf("%-24s NO ERROR\n", $label);
    }
}

echo "== runtime TypeErrors (fail-closed) ==\n";
function want_vec(vec[int] $v) {}
function want_tuple(tuple[int, string] $v) {}
function untyped($v) {}
$runtime = [
    'kind mismatch'   => fn() => want_vec(set{1, 2}),
    'untyped param'   => fn() => untyped(vec{1, 2}),
    'tuple too few'   => fn() => want_tuple(tuple{1}),
    'tuple too many'  => fn() => want_tuple(tuple{1, "a", 2}),
    'element mismatch'=> fn() => want_vec(vec{"x"}),
];
foreach ($runtime as $label => $fn) {
    try { $fn(); printf("%-18s NO ERROR\n", $label); }
    catch (TypeError $e) { printf("%-18s %s\n", $label, $e->getMessage()); }
}
?>
--EXPECTF--
== compile-time (each in its own process) ==
no source (assignment)   Cannot infer the element type of vec{}: no expected type is available here. Write vec[...]{...} to state the element type
no source (echo)         Cannot infer the element type of vec{}: no expected type is available here. Write vec[...]{...} to state the element type
no source (array elem)   Cannot infer the element type of vec{}: no expected type is available here. Write vec[...]{...} to state the element type
return not a collection  Cannot return vec{} where int is the declared return type
return wrong kind        Cannot return set{} where vec[int] is the declared return type
return no type           Cannot infer the element type of vec{}: the enclosing function declares no return type. Write vec[...]{...} to state the element type
return tuple arity       Collection type tuple expects 2 elements, 3 given
not implemented (map)    Collection type map is not implemented yet
const-expr position      %s
named argument           Cannot infer the element type of vec{} passed as a named argument. Write vec[...]{...} to state the element type
== runtime TypeErrors (fail-closed) ==
kind mismatch      Cannot use set{} where vec[int] is expected
untyped param      Cannot infer the element type of vec{}: the target parameter is not a constructible vec type. Write vec[...]{...} to state the element type
tuple too few      Collection type tuple expects 2 elements, 1 given
tuple too many     Collection type tuple expects 2 elements, 3 given
element mismatch   Element 0 of vec[int] must be of type int, string given
