<?php
function preloaded_literal_flat(): int {
    return zend_test_vec_count(vec[int]{1, 2, 3});
}

function preloaded_literal_nested() {
    return vec[vec[string]]{ vec[string]{'a'}, vec[string]{'b', 'c'} };
}

class PreloadedLiteralC {
    public function m(): string {
        try {
            $v = vec[int]{1, 'x'};
        } catch (TypeError $e) {
            return $e->getMessage();
        }
        return 'not rejected';
    }
}
