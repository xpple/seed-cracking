package bedrock;

import com.mojang.brigadier.CommandDispatcher;
import java.io.DataOutputStream;
import java.io.FileOutputStream;
import java.util.HashMap;
import net.fabricmc.api.ModInitializer;
import net.fabricmc.fabric.api.client.command.v2.ClientCommandRegistrationCallback;
import net.minecraft.world.level.block.state.BlockState;
import net.fabricmc.fabric.api.client.command.v2.ClientCommands;
import net.minecraft.client.Minecraft;
import net.minecraft.commands.Commands;
import net.minecraft.network.chat.Component;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.chunk.ChunkAccess;
import org.apache.logging.log4j.Logger;
import org.apache.logging.log4j.LogManager;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.ChunkPos;

public class Bedrockseed implements ModInitializer {
	public static final String MOD_ID = "bedrock-seed";

	// This logger is used to write text to the console and the log file.
	// It is considered best practice to use your mod id as the logger's name.
	// That way, it's clear which mod wrote info, warnings, and errors.
	public static final Logger LOGGER = LogManager.getLogger(MOD_ID);

	private Minecraft client = Minecraft.getInstance();

	@Override
	public void onInitialize() {
		// This code runs as soon as Minecraft is in a mod-load-ready state.
		// However, some things (like resources) may still be uninitialized.
		// Proceed with mild caution.

		LOGGER.info("Hello Fabric world!");
		ClientCommandRegistrationCallback.EVENT.register((dispatcher, registryAccess) -> dispatcher.register(ClientCommands.literal("savecrackdata")
			.executes(context -> {
				try {
					FileOutputStream fos = new FileOutputStream("seed_crack_data.dat", /*append=*/false);
					DataOutputStream dos = new DataOutputStream(fos);

					int px = (int)client.player.getX() >> 4, pz = (int)client.player.getZ() >> 4;
					for (int cx = px - 16; cx <= px + 16; cx++)
					for (int cz = pz - 16; cz <= pz + 16; cz++)
					{
						LOGGER.error("Process chunk: x=" + cx + " z=" + cz);
						ChunkAccess a = client.level.getChunk(new ChunkPos(cx, cz).getWorldPosition());
						HashMap<BlockState, Integer> states = new HashMap();
						states.put(Blocks.BEDROCK.getStateDefinition().any(), 0);
						states.put(Blocks.RAW_IRON_BLOCK.getStateDefinition().any(), 1);
						states.put(Blocks.RAW_COPPER_BLOCK.getStateDefinition().any(), 1);
						states.put(Blocks.DEEPSLATE_IRON_ORE.getStateDefinition().any(), 2);
						states.put(Blocks.COPPER_ORE.getStateDefinition().any(), 2);
						states.put(Blocks.TUFF.getStateDefinition().any(), 3);
						states.put(Blocks.GRANITE.getStateDefinition().any(), 3);
						HashMap<BlockState, Integer> statesFull = new HashMap(states);
						statesFull.put(Blocks.STONE.getStateDefinition().any(), 4);
						for(var st : Blocks.DEEPSLATE.getStateDefinition().getPossibleStates()) {
							statesFull.put(st, 5);
						}

						for(int x = 16 * cx; x < 16 * cx + 16; x++)
						for(int z = 16 * cz; z < 16 * cz + 16; z++)
						{
							for(int y = -63; y <= 50; y++)
							{
								BlockState b = a.getBlockState(new BlockPos(x, y, z));
								if(states.containsKey(b) || (y >= 1 && y <= 7 && statesFull.containsKey(b))) {
									//LOGGER.error("Got block: x=" + x + " y=" + y + " z=" + z + " " + b);
									dos.writeInt(x);
									dos.writeInt(y);
									dos.writeInt(z);
									dos.writeInt(statesFull.get(b));
								}
							}
						}
					}
					context.getSource().sendFeedback(Component.literal("Data saved to seed_crack_data.dat"));
				} catch(Exception e) {
					context.getSource().sendFeedback(Component.literal("Failed to write file"));
					LOGGER.error("Failed to write file!");
				}
				return 1;
			})));
	}
}
