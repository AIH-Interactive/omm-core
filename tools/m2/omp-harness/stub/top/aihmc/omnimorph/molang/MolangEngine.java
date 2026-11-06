package top.aihmc.omnimorph.molang;

import java.io.IOException;
import java.io.Reader;
import java.io.StringReader;
import java.util.List;

import top.aihmc.omnimorph.molang.parser.ParseException;
import top.aihmc.omnimorph.molang.parser.ast.Expression;

/** Harness stub with the same parse(String) contract as OMP's MolangEngine. */
public interface MolangEngine {
    List<Expression> parse(Reader reader) throws IOException;

    default List<Expression> parse(String str) throws ParseException {
        try (StringReader reader = new StringReader(str)) {
            return parse(reader);
        } catch (ParseException e) {
            throw e;
        } catch (IOException e) {
            throw new ParseException(e.getMessage());
        }
    }
}
