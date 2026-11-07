// @category Alenka
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import java.io.*;

public class ResourceAnalysis extends GhidraScript {
    public void run() throws Exception {
        Address base = currentProgram.getMinAddress();
        boolean lib = currentProgram.getName().endsWith(".LIB");
        String[] names = lib
            ? new String[]{"version", "origin_y", "max_y", "max_x_byte", "tile_width", "tile_height", "tile_columns", "tile_rows", "has_palette"}
            : new String[]{"version", "legacy_pointer", "cell_count_lo", "cell_count_hi", "columns", "rows"};
        File dir = new File(getScriptArgs()[0]);
        dir.mkdirs();
        try (PrintWriter out = new PrintWriter(new File(dir, currentProgram.getName()+".txt"), "UTF-8")) {
            out.println("Ghidra resource analysis: " + currentProgram.getName());
            for (int i=0; i<names.length; i++) {
                Address a = base.add(i*2);
                createWord(a);
                createLabel(a, names[i], true);
                out.printf("%s %s = %d%n", a, names[i], getShort(a)&0xffff);
            }
            if (lib) {
                for (int i=0; i<17; i++) {
                    Address a = base.add(18+i*6);
                    createLabel(a, "palette_"+i+"_rgb", true);
                    out.printf("palette[%d] = %d %d %d%n", i, getShort(a)&0xffff, getShort(a.add(2))&0xffff, getShort(a.add(4))&0xffff);
                }
                createLabel(base.add(256), "planes_3_2_1_0", true);
                setPlateComment(base.add(256), "Four planes, each 40*584 bytes; plane 3 first. Verified against EXE 1000:527a.");
            } else {
                createLabel(base.add(12), "GRED_Field_signature", true);
                createAsciiString(base.add(12));
                createLabel(base.add(32), "tile_addresses", true);
                setPlateComment(base.add(32), "80*274 uint16 entries. Subtract 16000 to get atlas byte address. EXE 1000:54f1.");
            }
        }
    }
}
