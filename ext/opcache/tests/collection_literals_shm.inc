<?php
/* Introspect collections through their own native surface (`->count`, `[i]`) so
 * the file-cache subprocess needs no extension -- zend_test is a shared DLL on
 * Windows and would not be loaded by the spawned `php` there. This also exercises
 * the real user-facing API across the cache round trip. */
function ocl_flat(): int {
    $v = vec[int]{1, 2, 3};
    return $v->count;
}

function ocl_nested(): int {
    $v = vec[vec[string]]{ vec[string]{'a'}, vec[string]{'b', 'c'} };
    return $v[1]->count;
}

function ocl_empty(): int {
    return (vec[int]{})->count;
}

/* A multi-member tuple descriptor: exercises a persisted descriptor with more
 * than one member type, and the positional element check after unserialization. */
function ocl_tuple(): string {
    $t = tuple[int, string, vec[int]]{1, 'a', vec[int]{2, 3}};
    return $t[0] . ',' . $t[1] . ',' . $t[2]->count;
}

function ocl_tuple_bad(): string {
    try {
        $t = tuple[int, string]{1, 2};
    } catch (TypeError $e) {
        return $e->getMessage();
    }
    return 'not rejected';
}

/* A persisted set descriptor, and its deduplication running after the script
 * has been unserialized from SHM. */
function ocl_set(): int {
    $s = set[int]{1, 2, 2, 3, 1};
    return $s->count;
}

/* Returned so a caller can check that repeated executions of a *persisted*
 * literal site keep borrowing the same canonical node. */
function ocl_make(int $n) {
    return vec[int]{$n, $n + 1};
}

class OclC {
    /* Two literal sites in one op_array, plus a class-typed element type, so
     * more than one descriptor table entry has to survive persistence. */
    public function pair(): array {
        $a = vec[int]{10};
        $b = vec[OclC]{$this};
        return [$a[0], $b->count];
    }

    public function bad(): string {
        try {
            $v = vec[int]{1, 'x'};
        } catch (TypeError $e) {
            return $e->getMessage();
        }
        return 'not rejected';
    }
}

function ocl_report(): string {
    $c = new OclC();
    return implode('|', [
        ocl_flat(),
        ocl_nested(),
        ocl_empty(),
        implode(',', $c->pair()),
        $c->bad(),
        ocl_tuple(),
        ocl_tuple_bad(),
        ocl_set(),
    ]);
}
