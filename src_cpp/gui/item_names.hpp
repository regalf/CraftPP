#pragma once

#include <string>

namespace craftpp::gui {

// Item/block display-name keys (Item/Block setItemName/setBlockName +
// en_US.lang), extracted from the 1.0 sources. Subtype overrides
// (dye/wool/coal/slab) follow getItemNameIS. Missing keys fall back
// to the key itself (StatCollector parity).
inline const char* name_key_for(int id, int damage) {
  if (id == 351) {  // dyePowder.<color>
    static const char* kColors[16] = {"black", "red", "green", "brown", "blue", "purple",
                                      "cyan", "silver", "gray", "pink", "lime", "yellow",
                                      "lightBlue", "magenta", "orange", "white"};
    static std::string k;
    int v = damage < 0 ? 0 : (damage > 15 ? 15 : damage);
    k = std::string("item.dyePowder.") + kColors[v] + ".name";
    return k.c_str();
  }
  if (id == 35) {  // tile.cloth.<color>, getBlockFromDye = ~dmg & 15
    static const char* kColors[16] = {"black", "red", "green", "brown", "blue", "purple",
                                      "cyan", "silver", "gray", "pink", "lime", "yellow",
                                      "lightBlue", "magenta", "orange", "white"};
    static std::string k;
    k = std::string("tile.cloth.") + kColors[(~damage) & 15] + ".name";
    return k.c_str();
  }
  if (id == 263) return damage == 1 ? "item.charcoal.name" : "item.coal.name";
  if (id == 43 || id == 44) {  // tile.stoneSlab.<variant>
    static const char* kVar[6] = {"stone", "sand", "wood", "cobble", "brick",
                                  "smoothStoneBrick"};
    static std::string k;
    int v = damage < 0 ? 0 : (damage > 5 ? 0 : damage);
    k = std::string("tile.stoneSlab.") + kVar[v] + ".name";
    return k.c_str();
  }
  switch (id) {
    case 1: return "tile.stone.name";
    case 3: return "tile.dirt.name";
    case 4: return "tile.stonebrick.name";
    case 5: return "tile.wood.name";
    case 6: return "tile.sapling.name";
    case 7: return "tile.bedrock.name";
    case 8: return "tile.water.name";
    case 9: return "tile.water.name";
    case 10: return "tile.lava.name";
    case 11: return "tile.lava.name";
    case 12: return "tile.sand.name";
    case 13: return "tile.gravel.name";
    case 14: return "tile.oreGold.name";
    case 15: return "tile.oreIron.name";
    case 16: return "tile.oreCoal.name";
    case 18: return "tile.leaves.name";
    case 20: return "tile.glass.name";
    case 21: return "tile.oreLapis.name";
    case 22: return "tile.blockLapis.name";
    case 27: return "tile.goldenRail.name";
    case 28: return "tile.detectorRail.name";
    case 29: return "tile.pistonStickyBase.name";
    case 30: return "tile.web.name";
    case 31: return "tile.tallgrass.name";
    case 32: return "tile.deadbush.name";
    case 33: return "tile.pistonBase.name";
    case 37: return "tile.flower.name";
    case 38: return "tile.rose.name";
    case 39: return "tile.mushroom.name";
    case 40: return "tile.mushroom.name";
    case 41: return "tile.blockGold.name";
    case 42: return "tile.blockIron.name";
    case 43: return "tile.stoneSlab.name";
    case 44: return "tile.stoneSlab.name";
    case 45: return "tile.brick.name";
    case 46: return "tile.tnt.name";
    case 47: return "tile.bookshelf.name";
    case 48: return "tile.stoneMoss.name";
    case 49: return "tile.obsidian.name";
    case 50: return "tile.torch.name";
    case 51: return "tile.fire.name";
    case 52: return "tile.mobSpawner.name";
    case 53: return "tile.stairsWood.name";
    case 55: return "tile.redstoneDust.name";
    case 56: return "tile.oreDiamond.name";
    case 57: return "tile.blockDiamond.name";
    case 59: return "tile.crops.name";
    case 61: return "tile.furnace.name";
    case 62: return "tile.furnace.name";
    case 63: return "tile.sign.name";
    case 64: return "tile.doorWood.name";
    case 65: return "tile.ladder.name";
    case 66: return "tile.rail.name";
    case 67: return "tile.stairsStone.name";
    case 68: return "tile.sign.name";
    case 69: return "tile.lever.name";
    case 70: return "tile.pressurePlate.name";
    case 71: return "tile.doorIron.name";
    case 72: return "tile.pressurePlate.name";
    case 73: return "tile.oreRedstone.name";
    case 74: return "tile.oreRedstone.name";
    case 75: return "tile.notGate.name";
    case 76: return "tile.notGate.name";
    case 77: return "tile.button.name";
    case 78: return "tile.snow.name";
    case 79: return "tile.ice.name";
    case 80: return "tile.snow.name";
    case 81: return "tile.cactus.name";
    case 82: return "tile.clay.name";
    case 83: return "tile.reeds.name";
    case 84: return "tile.jukebox.name";
    case 85: return "tile.fence.name";
    case 86: return "tile.pumpkin.name";
    case 87: return "tile.hellrock.name";
    case 88: return "tile.hellsand.name";
    case 89: return "tile.lightgem.name";
    case 90: return "tile.portal.name";
    case 91: return "tile.litpumpkin.name";
    case 92: return "tile.cake.name";
    case 93: return "tile.diode.name";
    case 94: return "tile.diode.name";
    case 96: return "tile.trapdoor.name";
    case 99: return "tile.mushroom.name";
    case 100: return "tile.mushroom.name";
    case 101: return "tile.fenceIron.name";
    case 102: return "tile.thinGlass.name";
    case 104: return "tile.pumpkinStem.name";
    case 105: return "tile.pumpkinStem.name";
    case 107: return "tile.fenceGate.name";
    case 108: return "tile.stairsBrick.name";
    case 109: return "tile.stairsStoneBrickSmooth.name";
    case 111: return "tile.waterlily.name";
    case 112: return "tile.netherBrick.name";
    case 113: return "tile.netherFence.name";
    case 114: return "tile.stairsNetherBrick.name";
    case 121: return "tile.whiteStone.name";
    case 122: return "tile.dragonEgg.name";
    case 256: return "item.shovelIron.name";
    case 257: return "item.pickaxeIron.name";
    case 258: return "item.hatchetIron.name";
    case 259: return "item.flintAndSteel.name";
    case 260: return "item.apple.name";
    case 261: return "item.bow.name";
    case 262: return "item.arrow.name";
    case 263: return "item.coal.name";
    case 264: return "item.emerald.name";
    case 265: return "item.ingotIron.name";
    case 266: return "item.ingotGold.name";
    case 267: return "item.swordIron.name";
    case 268: return "item.swordWood.name";
    case 269: return "item.shovelWood.name";
    case 270: return "item.pickaxeWood.name";
    case 271: return "item.hatchetWood.name";
    case 272: return "item.swordStone.name";
    case 273: return "item.shovelStone.name";
    case 274: return "item.pickaxeStone.name";
    case 275: return "item.hatchetStone.name";
    case 276: return "item.swordDiamond.name";
    case 277: return "item.shovelDiamond.name";
    case 278: return "item.pickaxeDiamond.name";
    case 279: return "item.hatchetDiamond.name";
    case 280: return "item.stick.name";
    case 281: return "item.bowl.name";
    case 282: return "item.mushroomStew.name";
    case 283: return "item.swordGold.name";
    case 284: return "item.shovelGold.name";
    case 285: return "item.pickaxeGold.name";
    case 286: return "item.hatchetGold.name";
    case 287: return "item.string.name";
    case 288: return "item.feather.name";
    case 289: return "item.sulphur.name";
    case 290: return "item.hoeWood.name";
    case 291: return "item.hoeStone.name";
    case 292: return "item.hoeIron.name";
    case 293: return "item.hoeDiamond.name";
    case 294: return "item.hoeGold.name";
    case 295: return "item.seeds.name";
    case 296: return "item.wheat.name";
    case 297: return "item.bread.name";
    case 298: return "item.helmetCloth.name";
    case 299: return "item.chestplateCloth.name";
    case 300: return "item.leggingsCloth.name";
    case 301: return "item.bootsCloth.name";
    case 302: return "item.helmetChain.name";
    case 303: return "item.chestplateChain.name";
    case 304: return "item.leggingsChain.name";
    case 305: return "item.bootsChain.name";
    case 306: return "item.helmetIron.name";
    case 307: return "item.chestplateIron.name";
    case 308: return "item.leggingsIron.name";
    case 309: return "item.bootsIron.name";
    case 310: return "item.helmetDiamond.name";
    case 311: return "item.chestplateDiamond.name";
    case 312: return "item.leggingsDiamond.name";
    case 313: return "item.bootsDiamond.name";
    case 314: return "item.helmetGold.name";
    case 315: return "item.chestplateGold.name";
    case 316: return "item.leggingsGold.name";
    case 317: return "item.bootsGold.name";
    case 318: return "item.flint.name";
    case 319: return "item.porkchopRaw.name";
    case 320: return "item.porkchopCooked.name";
    case 321: return "item.painting.name";
    case 322: return "item.appleGold.name";
    case 323: return "item.sign.name";
    case 324: return "item.doorWood.name";
    case 325: return "item.bucket.name";
    case 326: return "item.bucketWater.name";
    case 327: return "item.bucketLava.name";
    case 328: return "item.minecart.name";
    case 329: return "item.saddle.name";
    case 330: return "item.doorIron.name";
    case 331: return "item.redstone.name";
    case 332: return "item.snowball.name";
    case 333: return "item.boat.name";
    case 334: return "item.leather.name";
    case 335: return "item.milk.name";
    case 336: return "item.brick.name";
    case 337: return "item.clay.name";
    case 338: return "item.reeds.name";
    case 339: return "item.paper.name";
    case 340: return "item.book.name";
    case 341: return "item.slimeball.name";
    case 342: return "item.minecartChest.name";
    case 343: return "item.minecartFurnace.name";
    case 344: return "item.egg.name";
    case 345: return "item.compass.name";
    case 346: return "item.fishingRod.name";
    case 347: return "item.clock.name";
    case 348: return "item.yellowDust.name";
    case 349: return "item.fishRaw.name";
    case 350: return "item.fishCooked.name";
    case 351: return "item.dyePowder.name";
    case 352: return "item.bone.name";
    case 353: return "item.sugar.name";
    case 354: return "item.cake.name";
    case 355: return "item.bed.name";
    case 356: return "item.diode.name";
    case 357: return "item.cookie.name";
    case 358: return "item.map.name";
    case 359: return "item.shears.name";
    case 360: return "item.melon.name";
    case 361: return "item.seeds_pumpkin.name";
    case 362: return "item.seeds_melon.name";
    case 363: return "item.beefRaw.name";
    case 364: return "item.beefCooked.name";
    case 365: return "item.chickenRaw.name";
    case 366: return "item.chickenCooked.name";
    case 367: return "item.rottenFlesh.name";
    case 368: return "item.enderPearl.name";
    case 369: return "item.blazeRod.name";
    case 370: return "item.ghastTear.name";
    case 371: return "item.goldNugget.name";
    case 372: return "item.netherStalkSeeds.name";
    case 373: return "item.potion.name";
    case 374: return "item.glassBottle.name";
    case 375: return "item.spiderEye.name";
    case 376: return "item.fermentedSpiderEye.name";
    case 377: return "item.blazePowder.name";
    case 378: return "item.magmaCream.name";
    case 379: return "item.brewingStand.name";
    case 380: return "item.cauldron.name";
    case 381: return "item.eyeOfEnder.name";
    case 382: return "item.speckledMelon.name";
    case 2256: return "item.record.name";
    case 2257: return "item.record.name";
    case 2258: return "item.record.name";
    case 2259: return "item.record.name";
    case 2260: return "item.record.name";
    case 2261: return "item.record.name";
    case 2262: return "item.record.name";
    case 2263: return "item.record.name";
    case 2264: return "item.record.name";
    case 2265: return "item.record.name";
    case 2266: return "item.record.name";
    default: return "";
  }
}

}  // namespace craftpp::gui
