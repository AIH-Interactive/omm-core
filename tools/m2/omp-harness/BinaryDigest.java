import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.HexFormat;
import java.util.List;
import java.util.Map;

import top.aihmc.omnimorph.avatar.ysm.binary.YsmBinaryModel;

/**
 * Canonical digest of an OMP YsmBinaryModel. The token stream (and therefore the digest)
 * is mirrored exactly by tests/support/binary_digest.hpp; any field difference between
 * the Java reference and the native reader changes the SHA-256.
 *
 * Tokens: S<len>:<utf8>  I<decimal>;  F<raw float bits hex>;  B<sha256 of bytes>;  N (null)
 */
final class BinaryDigest {
    private final MessageDigest digest;

    private BinaryDigest() throws Exception {
        digest = MessageDigest.getInstance("SHA-256");
    }

    static String of(YsmBinaryModel model) throws Exception {
        BinaryDigest writer = new BinaryDigest();
        writer.model(model);
        return HexFormat.of().formatHex(writer.digest.digest());
    }

    private void raw(String text) {
        digest.update(text.getBytes(StandardCharsets.UTF_8));
    }

    private void s(String value) {
        if (value == null) {
            raw("N");
            return;
        }
        byte[] bytes = value.getBytes(StandardCharsets.UTF_8);
        raw("S" + bytes.length + ":");
        digest.update(bytes);
    }

    private void i(long value) {
        raw("I" + value + ";");
    }

    private void b(boolean value) {
        i(value ? 1 : 0);
    }

    private void f(float value) {
        raw("F" + Integer.toHexString(Float.floatToRawIntBits(value)) + ";");
    }

    private void bytes(byte[] data) throws Exception {
        if (data == null) {
            raw("N");
            return;
        }
        raw("B" + HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(data)) + ";");
    }

    private void value(Object value) {
        if (value == null) {
            raw("N");
        } else if (value instanceof Float number) {
            raw("V");
            f(number);
        } else {
            raw("T");
            s((String) value);
        }
    }

    private void map(Map<String, String> values) {
        i(values.size());
        for (Map.Entry<String, String> entry : values.entrySet()) {
            s(entry.getKey());
            s(entry.getValue());
        }
    }

    private void list(List<String> values) {
        i(values.size());
        for (String value : values) {
            s(value);
        }
    }

    private void image(YsmBinaryModel.RawImage image) throws Exception {
        if (image == null) {
            raw("N");
            return;
        }
        s(image.name);
        bytes(image.data);
        i(image.width);
        i(image.height);
        i(image.format);
        i(image.unknownFlag);
    }

    private void model(YsmBinaryModel model) throws Exception {
        i(model.formatVersion);

        YsmBinaryModel.RawMetadata metadata = model.metadata;
        s(metadata.name);
        s(metadata.tips);
        s(metadata.licenseType);
        s(metadata.licenseDescription);
        i(metadata.authors.size());
        for (YsmBinaryModel.RawMetadata.Author author : metadata.authors) {
            s(author.name);
            s(author.role);
            s(author.comment);
            map(author.contacts);
            s(author.avatar);
            image(author.avatarImage);
        }
        map(metadata.links);
        i(metadata.extraAvatars.size());
        for (YsmBinaryModel.RawImage image : metadata.extraAvatars) {
            image(image);
        }

        YsmBinaryModel.RawProperties properties = model.properties;
        s(properties.sha256);
        f(properties.widthScale);
        f(properties.heightScale);
        s(properties.defaultTexture);
        s(properties.previewAnimation);
        b(properties.isFree);
        b(properties.renderLayersFirst);
        b(properties.allCutout);
        b(properties.disablePreviewRotation);
        b(properties.guiNoLighting);
        b(properties.mergeMultilineExpr);
        s(properties.guiForeground);
        s(properties.guiBackground);
        i(properties.backgroundImages.size());
        for (YsmBinaryModel.RawImage image : properties.backgroundImages) {
            image(image);
        }
        map(properties.extraAnimations);
        i(properties.extraAnimationClassifies.size());
        for (YsmBinaryModel.ExtraAnimationClassify classify : properties.extraAnimationClassifies) {
            s(classify.id);
            map(classify.extras);
        }
        i(properties.extraAnimationButtons.size());
        for (YsmBinaryModel.ExtraAnimationButton button : properties.extraAnimationButtons) {
            s(button.id);
            s(button.name);
            i(button.forms.size());
            for (YsmBinaryModel.ConfigForm form : button.forms) {
                s(form.type);
                s(form.title);
                s(form.description);
                s(form.defaultValue);
                f(form.step);
                f(form.min);
                f(form.max);
                map(form.labels);
            }
        }

        geometry(model.mainEntity.mainModel);
        geometry(model.mainEntity.armModel);
        textures(model.mainEntity.textures);
        animationFiles(model.mainEntity.animationFiles);
        controllers(model.mainEntity.animationControllerFiles);

        subEntities(model.vehicles);
        subEntities(model.projectiles);

        dataFiles(model.soundFiles);
        dataFiles(model.functionFiles);
        i(model.languageFiles.size());
        for (Map.Entry<String, YsmBinaryModel.RawLanguageFile> entry : model.languageFiles.entrySet()) {
            s(entry.getKey());
            s(entry.getValue().hash);
            map(entry.getValue().data);
        }

        YsmBinaryModel.RawFooter footer = model.footer;
        i(footer.version);
        i(footer.unkInt1);
        s(footer.rand);
        i(footer.time);
        s(footer.extra);
        i(footer.unkInt2);
    }

    private void subEntities(Map<String, YsmBinaryModel.RawSubEntity> entities) throws Exception {
        i(entities.size());
        for (Map.Entry<String, YsmBinaryModel.RawSubEntity> entry : entities.entrySet()) {
            s(entry.getKey());
            YsmBinaryModel.RawSubEntity sub = entry.getValue();
            s(sub.identifier);
            if (sub.matchIds == null) {
                raw("N");
            } else {
                list(List.of(sub.matchIds));
            }
            geometry(sub.model);
            textures(sub.textures);
            animationFiles(sub.animationFiles);
            controllers(sub.animationControllerFiles);
        }
    }

    private void dataFiles(Map<String, YsmBinaryModel.RawDataFile> files) throws Exception {
        i(files.size());
        for (Map.Entry<String, YsmBinaryModel.RawDataFile> entry : files.entrySet()) {
            s(entry.getKey());
            s(entry.getValue().hash);
            bytes(entry.getValue().data);
        }
    }

    private void vec(float[] values) {
        for (float value : values) {
            f(value);
        }
    }

    private void geometry(YsmBinaryModel.RawGeometry geometry) {
        if (geometry == null) {
            raw("N");
            return;
        }
        i(geometry.modelType);
        s(geometry.identifier);
        s(geometry.sha256);
        f(geometry.textureWidth);
        f(geometry.textureHeight);
        f(geometry.visibleBoundsWidth);
        f(geometry.visibleBoundsHeight);
        i(geometry.visibleBoundsOffset.length);
        vec(geometry.visibleBoundsOffset);
        f(geometry.unkFloat1);
        f(geometry.unkFloat2);
        i(geometry.footerPad1);
        i(geometry.footerPad2);
        i(geometry.footerPad3);
        i(geometry.bones.size());
        for (YsmBinaryModel.RawBone bone : geometry.bones) {
            s(bone.name);
            s(bone.parentName);
            vec(bone.pivot);
            vec(bone.rotation);
            i(bone.unkPad1);
            i(bone.unkPad2);
            i(bone.unkPad3);
            i(bone.unkPad4);
            i(bone.unkPad5);
            i(bone.cubes.size());
            for (YsmBinaryModel.RawCube cube : bone.cubes) {
                i(cube.faces.size());
                for (YsmBinaryModel.RawFace face : cube.faces) {
                    vec(face.normal);
                    for (int vertex = 0; vertex < 4; vertex++) {
                        vec(face.positions[vertex]);
                        f(face.u[vertex]);
                        f(face.v[vertex]);
                    }
                }
                i(cube.unkInt1);
                i(cube.unkInt2);
                i(cube.unkInt3);
            }
        }
    }

    private void textures(Map<String, YsmBinaryModel.RawTexture> textures) throws Exception {
        i(textures.size());
        for (Map.Entry<String, YsmBinaryModel.RawTexture> entry : textures.entrySet()) {
            s(entry.getKey());
            YsmBinaryModel.RawTexture texture = entry.getValue();
            s(texture.name);
            s(texture.hash);
            i(texture.width);
            i(texture.height);
            i(texture.imageFormat);
            bytes(texture.data);
            i(texture.unknownFlag);
            i(texture.subTextures.size());
            for (YsmBinaryModel.RawTexture.SubTexture sub : texture.subTextures) {
                s(sub.hash);
                i(sub.specularType);
                i(sub.width);
                i(sub.height);
                i(sub.imageFormat);
                bytes(sub.data);
                i(sub.unknownFlag);
            }
        }
    }

    private void channel(List<YsmBinaryModel.RawKeyframe> frames) {
        i(frames.size());
        for (YsmBinaryModel.RawKeyframe frame : frames) {
            f(frame.timestamp);
            i(frame.interpolationMode);
            for (Object item : frame.postData) {
                value(item);
            }
            b(frame.hasPreData);
            if (frame.hasPreData) {
                for (Object item : frame.preData) {
                    value(item);
                }
            }
        }
    }

    private void animationFiles(Map<String, YsmBinaryModel.RawAnimationFile> files) {
        i(files.size());
        for (Map.Entry<String, YsmBinaryModel.RawAnimationFile> entry : files.entrySet()) {
            s(entry.getKey());
            YsmBinaryModel.RawAnimationFile file = entry.getValue();
            i(file.animType);
            s(file.fileHash);
            i(file.animations.size());
            for (Map.Entry<String, YsmBinaryModel.RawAnimation> clipEntry : file.animations.entrySet()) {
                s(clipEntry.getKey());
                YsmBinaryModel.RawAnimation clip = clipEntry.getValue();
                s(clip.name);
                f(clip.length);
                i(clip.loopMode);
                value(clip.blendWeight);
                i(clip.unkInt1);
                i(clip.unkInt2);
                i(clip.unkInt4);
                i(clip.boneAnimations.size());
                for (YsmBinaryModel.RawBoneAnimation bone : clip.boneAnimations) {
                    s(bone.boneName);
                    channel(bone.rotation);
                    channel(bone.position);
                    channel(bone.scale);
                }
                i(clip.timelineEvents.size());
                for (YsmBinaryModel.RawTimelineEvent event : clip.timelineEvents) {
                    f(event.timestamp);
                    list(event.events);
                }
                i(clip.soundEffects.size());
                for (YsmBinaryModel.RawSoundEffect sound : clip.soundEffects) {
                    s(sound.effectName);
                    f(sound.timestamp);
                }
            }
        }
    }

    private void controllers(List<YsmBinaryModel.RawAnimationControllerFile> files) {
        i(files.size());
        for (YsmBinaryModel.RawAnimationControllerFile file : files) {
            s(file.name);
            s(file.hash);
            i(file.legacyUnknownInt);
            i(file.controllers.size());
            for (Map.Entry<String, YsmBinaryModel.RawAnimationController> entry : file.controllers.entrySet()) {
                s(entry.getKey());
                YsmBinaryModel.RawAnimationController controller = entry.getValue();
                s(controller.animationName);
                s(controller.initialState);
                i(controller.states.size());
                for (YsmBinaryModel.RawControllerState state : controller.states) {
                    s(state.name);
                    map(state.animations);
                    map(state.transitions);
                    list(state.onEntry);
                    list(state.onExit);
                    f(state.blendTransitionValue);
                    b(state.blendViaShortestPath);
                    i(state.blendTransitions.size());
                    for (Map.Entry<Float, Float> point : state.blendTransitions.entrySet()) {
                        f(point.getKey());
                        f(point.getValue());
                    }
                    list(state.soundEffects);
                }
            }
        }
    }
}
