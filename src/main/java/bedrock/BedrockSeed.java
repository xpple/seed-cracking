package bedrock;

import bedrock.command.BedrockCrackCommand;
import bedrock.generated.BedrockNative;
import com.mojang.brigadier.CommandDispatcher;
import net.fabricmc.api.ClientModInitializer;
import net.fabricmc.fabric.api.client.command.v2.ClientCommandRegistrationCallback;
import net.fabricmc.fabric.api.client.command.v2.FabricClientCommandSource;
import net.fabricmc.loader.api.FabricLoader;
import net.fabricmc.loader.api.ModContainer;
import net.minecraft.commands.CommandBuildContext;
import org.apache.logging.log4j.LogManager;
import org.apache.logging.log4j.Logger;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;

public class BedrockSeed implements ClientModInitializer {
	public static final String MOD_ID = "bedrock-seed";

	public static final Logger LOGGER = LogManager.getLogger(MOD_ID);

	static {
		String z3LibraryName = System.mapLibraryName("z3");
		z3LibraryName = z3LibraryName.startsWith("lib") ? z3LibraryName : "lib" + z3LibraryName;
		String libraryName = System.mapLibraryName("crack");
		ModContainer modContainer = FabricLoader.getInstance().getModContainer(MOD_ID).orElseThrow();
		Path z3Path, crackPath;
		try {
			Path tempDir = Files.createTempDirectory("crack");
			z3Path = tempDir.resolve(z3LibraryName);
			crackPath = tempDir.resolve(libraryName);
			Files.copy(modContainer.findPath(z3LibraryName).orElseThrow(), z3Path, StandardCopyOption.REPLACE_EXISTING);
			Files.copy(modContainer.findPath(libraryName).orElseThrow(), crackPath, StandardCopyOption.REPLACE_EXISTING);
		} catch (IOException e) {
			throw new RuntimeException(e);
		}
		System.load(z3Path.toAbsolutePath().toString());
		System.load(crackPath.toAbsolutePath().toString());
		// verify library is loaded successfully
        //noinspection ResultOfMethodCallIgnored
		BedrockNative.crack$address();
	}

	@Override
	public void onInitializeClient() {
		ClientCommandRegistrationCallback.EVENT.register(BedrockSeed::registerCommands);
		BlockDataManager.registerEvents();
	}

	private static void registerCommands(CommandDispatcher<FabricClientCommandSource> dispatcher, CommandBuildContext commandBuildContext) {
		BedrockCrackCommand.register(dispatcher);
	}
}
