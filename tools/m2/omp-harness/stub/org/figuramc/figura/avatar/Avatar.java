package org.figuramc.figura.avatar;

import top.aihmc.omnimorph.molang.MolangEngine;

/** Harness stub: only the Molang engine accessor used by the YSM parsers. */
public final class Avatar {
    private static final MolangEngine ENGINE = new TextMolangEngine();

    private Avatar() {
    }

    public static MolangEngine getMolangEngine() {
        return ENGINE;
    }

    /** Records the expression text instead of compiling Molang. */
    static final class TextMolangEngine implements MolangEngine {
        @Override
        public java.util.List<top.aihmc.omnimorph.molang.parser.ast.Expression> parse(java.io.Reader reader)
                throws java.io.IOException {
            StringBuilder text = new StringBuilder();
            char[] buffer = new char[1024];
            int count;
            while ((count = reader.read(buffer)) != -1) {
                text.append(buffer, 0, count);
            }
            return java.util.List.of(new top.aihmc.omnimorph.molang.parser.ast.Expression.Text(text.toString()));
        }
    }
}
