function vpv_update(id, varargin)
    j=1;
    for i=1:nargin-1
        o=varargin{i};
        if ischar(o)
        else
            dir=[tempdir 'matlab_' num2str(id) '/'];
            if ~exist(dir, 'dir')
                mkdir(dir);
            end
            name=[dir num2str(j) '.tiff'];
            imwrite_with_tiff(o, name);
            j=j+1;
        end
    end
end

function imwrite_with_tiff(img, filename)
    [~, ~, ext] = fileparts(filename);
    if strcmp(ext, '.tiff')
       t = Tiff(filename, 'w');
       tagstruct.ImageLength = size(img, 1);
       tagstruct.ImageWidth = size(img, 2);
       tagstruct.Compression = Tiff.Compression.None;
       %tagstruct.Compression = Tiff.Compression.LZW;        % compressed
       tagstruct.SampleFormat = Tiff.SampleFormat.IEEEFP;
       tagstruct.Photometric = Tiff.Photometric.MinIsBlack;
       tagstruct.BitsPerSample =  32;                        % float data
       tagstruct.SamplesPerPixel = size(img,3);
       tagstruct.PlanarConfiguration = Tiff.PlanarConfiguration.Chunky;
       t.setTag(tagstruct);
       t.write(single(img));
       t.close();
    else
        imwrite(img, filename);
    end
end


